/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "pcache.h"
#include "pint-bucket.h"
#include "pint-dcache.h"
#include "config-manage.h"
#include "pvfs2-sysint.h"
#include "pint-sysint.h"
#include "gen-locks.h"
#include "pint-servreq.h"
#include "PINT-reqproto-encode.h"

#define BKT_STR_SIZE 7

#define REQ_ENC_FORMAT 0

static int server_send_req(bmi_addr_t addr,
			   struct PVFS_server_req_s *req_p,
			   PVFS_credentials creds,
			   struct PVFS_server_resp_s **ack_pp);

/* pinode cache */
extern pcache pvfs_pcache; 
/* PVFS directory cache */
extern struct dcache pvfs_dcache;
extern fsconfig_array server_config;

extern gen_mutex_t *g_session_tag_mt_lock;

static int server_get_config(pvfs_mntlist mntent_list);

/* PVFS_sys_initialize()
 *
 * Initializes the PVFS system interface and any necessary internal data
 * structures.  Must be called before any other system interface function.
 *
 * returns 0 on success, -errno on failure
 */

int PVFS_sys_initialize(pvfs_mntlist mntent_list)
{
    int ret = -1;
    gen_mutex_t mt_config;

    enum {
	NONE_INIT_FAIL = 0,
	BMI_INIT_FAIL,
	FLOW_INIT_FAIL,
	JOB_INIT_FAIL,
	PCACHE_INIT_FAIL,
	DCACHE_INIT_FAIL,
	BUCKET_INIT_FAIL,
	GET_CONFIG_INIT_FAIL
    } init_fail = NONE_INIT_FAIL; /* used for cleanup in the event of failures */

    /* Initialize BMI */
    /*TODO: change this so it parses the bmi module from the pvfstab file*/
    ret = BMI_initialize("bmi_tcp",NULL,0);
    if (ret < 0)
    {
	init_fail = BMI_INIT_FAIL;
	printf("BMI initialize failure\n");
	goto return_error;
    }

    /* initialize bmi session identifier, TODO: DOCUMENT THIS */
    g_session_tag_mt_lock = gen_mutex_build();

    /* Initialize flow */
    ret = PINT_flow_initialize("flowproto_bmi_trove", 0);
    if (ret < 0)
    {
	init_fail = FLOW_INIT_FAIL;
	printf("Flow initialize failure.\n");
	goto return_error;
    }

    /* Initialize the job interface */
    ret = job_initialize(0);
    if (ret < 0)
    {
	init_fail = JOB_INIT_FAIL;
	printf("Error initializing job interface: %s\n",strerror(-ret));
	goto return_error;
    }
	
    /* Initialize the pinode cache */
    ret = pcache_initialize(&pvfs_pcache);
    if (ret < 0)
    {
	init_fail = PCACHE_INIT_FAIL;
	printf("Error initializing pinode cache\n");
	goto return_error;	
    }

    /* Initialize the directory cache */
    ret = dcache_initialize(&pvfs_dcache);
    if (ret < 0)
    {
	init_fail = DCACHE_INIT_FAIL;
	printf("Error initializing directory cache\n");
	goto return_error;	
    }	

    server_config.nr_fs = mntent_list.nr_entry;
    server_config.fs_info = (fsconfig *)malloc(mntent_list.nr_entry * sizeof(fsconfig));
    if (server_config.fs_info == NULL)
    {
	assert(0);
	printf("Error in allocating configuration parameters\n");
	ret = -ENOMEM;
	goto return_error;
    }

    /* Grab the mutex - serialize all writes to server_config */
    gen_mutex_lock(&mt_config);	
	
    /* Initialize the configuration management interface */
    ret = PINT_bucket_initialize();
    if (ret < 0)
    {
	init_fail = BUCKET_INIT_FAIL;
	printf("Error in initializing configuration management interface\n");
	/* Release the mutex */
	gen_mutex_unlock(&mt_config);
	goto return_error;
    }

    /* Get configuration parameters from server */
    ret = server_get_config(mntent_list);
    if (ret < 0)
    {
	init_fail = GET_CONFIG_INIT_FAIL;
	printf("Error in getting server config parameters\n");
	/* Release the mutex */
	gen_mutex_unlock(&mt_config);
	goto return_error;
    }	

    /* Release the mutex */
    gen_mutex_unlock(&mt_config);
	
    /* load the server info from config file to table */
    return(0);

 return_error:
    free(server_config.fs_info);

    switch(init_fail) {
	case GET_CONFIG_INIT_FAIL:
	    PINT_bucket_finalize();
	case BUCKET_INIT_FAIL:
	    dcache_finalize(&pvfs_dcache);
	case DCACHE_INIT_FAIL:
	    pcache_finalize(pvfs_pcache);
	case PCACHE_INIT_FAIL:
	    job_finalize();
	case JOB_INIT_FAIL:
	    PINT_flow_finalize();
	case FLOW_INIT_FAIL:
	    BMI_finalize();
	case BMI_INIT_FAIL:
	case NONE_INIT_FAIL:
	    /* nothing to do for either of these */
    }
    return(ret);
}

/* server_get_config()
 *
 */
static int server_get_config(pvfs_mntlist mntent_list)
{
    struct PVFS_server_req_s *req_p = NULL;		/* server request */
    struct PVFS_server_resp_s *ack_p = NULL;	/* server response */
    int ret = -1, i = 0, j = 0;
    bmi_addr_t serv_addr;				 /*PVFS address type structure*/ 
    int start = 0, k = 0, name_sz = 0, metalen= 0, iolen = 0;
    PVFS_credentials creds;

    enum {
	NONE_FAIL = 0,
	LOOKUP_FAIL,
	ALLOC_NAME_FAIL,
    } failure = NONE_FAIL;

    req_p = (struct PVFS_server_req_s *) malloc(sizeof(struct PVFS_server_req_s));
    if (req_p == NULL) {
	assert(0);
    }

    /* TODO: Fill up the credentials information */

    /* Process all entries in pvfstab */
    for (i = 0; i < mntent_list.nr_entry; i++) 
    {
	pvfs_mntent *mntent_p = &mntent_list.ptab_p[i]; /* for convenience */
	struct fsconfig_s *fsinfo_p;

   	/* Obtain the metaserver to send the request */
	ret = BMI_addr_lookup(&serv_addr, mntent_p->meta_addr);   
	if (ret < 0)
	{
	    failure = LOOKUP_FAIL;
	    goto return_error;
	}


	/* Set up the request for getconfig */
	name_sz = strlen(mntent_p->serv_mnt_dir) + 1;

	req_p->op                      = PVFS_SERV_GETCONFIG;	
	req_p->rsize                   = sizeof(struct PVFS_server_req_s) + name_sz;
	req_p->credentials             = creds;
	req_p->u.getconfig.fs_name     = mntent_p->serv_mnt_dir; /* just point to the mount info */
	req_p->u.getconfig.max_strsize = MAX_STRING_SIZE;


	/* DO THE GETCONFIG */
	/* do_getconfig() fills returns an allocated and populated PVFS_server_resp_s
	 * pointed to by ack_p.
	 */
	ret = server_send_req(serv_addr, req_p, creds, &ack_p);
#if 0
	ret = pint_serv_getconfig(&req_p, &ack_p, &req_getconfig, cred, &serv_addr);
	if (ret < 0)
	{
	    ret = -EINVAL;
	    goto getconfig_failure;
	}
#endif

	/* Use data from response to update global tables */
	fsinfo_p = &server_config.fs_info[i];

	fsinfo_p->fh_root         = ack_p->u.getconfig.root_handle;
	fsinfo_p->fsid            = ack_p->u.getconfig.fs_id;
	fsinfo_p->meta_serv_count = ack_p->u.getconfig.meta_server_count;
	fsinfo_p->io_serv_count   = ack_p->u.getconfig.io_server_count;
	fsinfo_p->maskbits        = ack_p->u.getconfig.maskbits;

	/* Copy the client mount point */
	name_sz = strlen(mntent_p->local_mnt_dir) + 1;
	fsinfo_p->local_mnt_dir = (PVFS_string) malloc(name_sz);
	if (fsinfo_p->local_mnt_dir == NULL) {
	    return -1;
	}
	memcpy(fsinfo_p->local_mnt_dir, mntent_p->local_mnt_dir, name_sz);

	metalen = strlen(ack_p->u.getconfig.meta_server_mapping);
	iolen   = strlen(ack_p->u.getconfig.io_server_mapping);

	printf("meta: %s\nio: %s\n",
	       ack_p->u.getconfig.meta_server_mapping,
	       ack_p->u.getconfig.io_server_mapping);

	/* How to get the size of metaserver list in ack? */
	/* NOTE: PVFS_string == char *, SO I HAVE DOUBTS ABOUT THIS LINE!!! -- ROB */
	fsinfo_p->meta_serv_array = (PVFS_string *) malloc(fsinfo_p->meta_serv_count * sizeof(PVFS_string));
	if (fsinfo_p->meta_serv_array == NULL)
	{
	    ret = -ENOMEM;
	    goto getconfig_failure;
	}
	fsinfo_p->bucket_array = (bucket_info *) malloc(fsinfo_p->meta_serv_count * sizeof(bucket_info));
	if (fsinfo_p->bucket_array == NULL)
	{
	    ret = -ENOMEM;
	    goto metabucketalloc_failure;
	}
	/* Copy the metaservers from ack to config info */
	for(j = 0; j < fsinfo_p->meta_serv_count; j++) 
	{
	    k = start;
	    while (ack_p->u.getconfig.meta_server_mapping[k] != '-' && k < metalen) k++;
	    start = k;
	    fsinfo_p->meta_serv_array[j] = (PVFS_string) malloc(k - start + 1);
	    if (fsinfo_p->meta_serv_array[j] == NULL)
	    {
		ret = -ENOMEM;
		goto metaserver_alloc_failure;
	    }
	    memcpy(fsinfo_p->meta_serv_array[j], &(ack_p->u.getconfig.meta_server_mapping[start]),k-start);
	    fsinfo_p->meta_serv_array[j][k-start] = '\0';

	    /*** NO BUCKET TABLE INFO ON THE WIRE AT THIS TIME ***/
	}
	start = k = 0;

	/* How to get the size of ioserver list in ack? */
	fsinfo_p->io_serv_array = (PVFS_string *) malloc(fsinfo_p->io_serv_count * sizeof(PVFS_string));
	if (fsinfo_p->io_serv_array == NULL)
	{
	    ret = -ENOMEM;
	    goto metaserver_alloc_failure;
	}
	fsinfo_p->io_bucket_array = (bucket_info *) malloc(fsinfo_p->io_serv_count * sizeof(bucket_info));
	if (fsinfo_p->io_bucket_array == NULL)
	{
	    ret = -ENOMEM;
	    goto iobucketalloc_failure;
	}
	/* Copy the ioservers from ack to config info */
	for(j = 0; j < fsinfo_p->io_serv_count; j++) 
	{
	    k = start;
	    /* CHANGE THE SPACE BELOW TO A DASH SO IT WOULD SKIP pvfs- */
	    while (ack_p->u.getconfig.io_server_mapping[k] != '-' && k < iolen) k++;
	    /* ASSIGN start = k TO SKIP pvfs- */
	    start = k;
	    fsinfo_p->io_serv_array[j] = (PVFS_string)malloc(k - start + 1);
	    if (fsinfo_p->io_serv_array[j] == NULL)
	    {
		ret = -ENOMEM;
		goto ioserver_alloc_failure;
	    }
	    memcpy(fsinfo_p->io_serv_array[j], &(ack_p->u.getconfig.io_server_mapping[start]),k-start);
	    fsinfo_p->io_serv_array[j][k-start] = '\0';

	    /*** NO BUCKET STUFF IS ON THE WIRE AT THIS TIME ***/


	}
	start = k = 0;

#if 0
	sysjob_free(serv_addr,req_p,req_size,BMI_SEND_BUFFER,NULL);
	sysjob_free(serv_addr,ack_p,ack_size,BMI_RECV_BUFFER,NULL);
#endif
    }
    return(0); 


    /* TODO: FIX UP ERROR HANDLING; NEEDS TO BE REVIEWED TOTALLY */
 ioserver_alloc_failure:
 iobucketalloc_failure:
 metaserver_alloc_failure:
 metabucketalloc_failure:
 getconfig_failure:
 namealloc_failure:
 return_error:
    return(ret);
}	  

/* server_send_req()
 *
 * TODO: PREPOST RECV
 */
static int server_send_req(bmi_addr_t addr,
			   struct PVFS_server_req_s *req_p,
			   PVFS_credentials creds,
			   struct PVFS_server_resp_s **ack_pp)
{
    int ret;
    bmi_size_t max_msg_sz;
    struct PINT_encoded_msg encoded;
    struct PINT_decoded_msg decoded;
    char *encoded_resp;
    job_status_s s_status, r_status;
    PVFS_msg_tag_t op_tag = get_next_session_tag();

    /* convert into something we can send across the wire.
     *
     * PINT_encode returns an encoded buffer in encoded. We have to free it
     * later.
     */
    ret = PINT_encode(req_p, PINT_ENCODE_REQ, &encoded, addr, REQ_ENC_FORMAT);

    /* TODO: IS THIS A REASONABLE MAXIMUM MESSAGE SIZE?  I HAVE NO IDEA */
    max_msg_sz = sizeof(struct PVFS_server_resp_s) + 2 * req_p->u.getconfig.max_strsize;

    /* allocate space for response, prepost receive */
    encoded_resp = BMI_memalloc(addr, max_msg_sz, BMI_RECV_BUFFER);
    if (encoded_resp == NULL)
    {
	ret = -ENOMEM;
	goto return_error;
    }

    /* post a blocking send job (this is a helper function) */
    ret = job_bmi_send_blocking(addr,
				encoded.buffer_list[0],
				encoded.size_list[0],
				op_tag,
				BMI_PRE_ALLOC,
				1, /* # of items in lists */
				&s_status);
    if (ret < 0)
    {
	goto return_error;
    }
    else if (ret == 1 && s_status.error_code != 0)
    {
	ret = -EINVAL;
	goto return_error;
    }

    /* post a blocking receive job */
    ret = job_bmi_recv_blocking(addr, encoded_resp, max_msg_sz, op_tag, BMI_PRE_ALLOC, &r_status);
    if (ret < 0)
    {
	goto return_error;
    }
    else if (ret == 1 && (r_status.error_code != 0 || r_status.actual_size > max_msg_sz))
    {
	ret = -EINVAL;
	goto return_error;
    }

    /* decode msg from wire format here; function allocates space for decoded response.
     * PINT_decode_release() must be used to free this later.
     */
    ret = PINT_decode(encoded_resp,
		      PINT_DECODE_RESP,
		      &decoded,
		      addr,
		      r_status.actual_size,
		      NULL);
    if (ret < 0)
    {
	ret = (-EINVAL);
	goto return_error;
    }

    /* free encoded_resp buffer */
    ret = BMI_memfree(addr, encoded_resp, max_msg_sz, BMI_RECV_BUFFER);
    assert(ret == 0);

    *ack_pp = (struct PVFS_server_resp_s *) decoded.buffer;
    return 0;
 return_error:
    return -1;
}
		

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
