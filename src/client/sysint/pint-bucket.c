/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */

#ifndef __PINT_BUCKET_H
#define __PINT_BUCKET_H

#include <errno.h>

#include <pvfs2-types.h>
#include <bmi.h>
#include <gossip.h>

static char HACK_server_name[] = "tcp://localhost:3334";
static PVFS_handle HACK_handle_mask = 0;
static PVFS_fs_id HACK_fsid = 0;
static PVFS_handle HACK_bucket = 0;

/* TODO: NOTE: THIS IS NOT A FULL IMPLEMENTATION.  It is simply a stub that
 * can operate on a single server file system for testing purposes.
 */

/* This is a prototype implementation of the bucket management component
 * of the system interface.  It is responsible for managing the list of meta
 * and i/o servers and mapping between buckets and servers.
 */

/* PINT_bucket_initialize()
 *
 * initializes the bucket interface
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_initialize(void)
{
	return(0);
}

/* PINT_bucket_finalize()
 *
 * shuts down the bucket interface and releases any resources associated
 * with it.
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_finalize(void)
{
	return(0);
}

/* PINT_bucket_load_mapping()
 *
 * loads a new mapping of servers to buckets into the bucket interface.
 * This function may be called multiple times in order to add new file
 * systems at run time.
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_load_mapping(
	char* meta_mapping,
	int meta_count,
	char* io_mapping,
	int io_count, 
	PVFS_handle handle_mask,
	PVFS_fs_id fsid)
{
	/* ignore the actual mapping for now */
	gossip_lerr("Warning: PINT_bucket_load_mapping() ignoring map and\n");
	gossip_lerr("Warning:    using %s as the only server.\n",
		HACK_server_name);

	if(meta_count != 1 || io_count != 1)
	{
		gossip_lerr("Error: PINT_bucket can only handle one server.\n");
		return(-EINVAL);
	}

	/* but stash the handle mask and fsid */
	HACK_handle_mask = handle_mask;
	HACK_fsid = fsid;

	return(0);
}


/* PINT_bucket_get_next_meta()
 *
 * returns the address, bucket, and handle mask of the next server 
 * that should be used to store a new piece of metadata.  This 
 * function is responsible for fairly distributing the metadata 
 * storage responsibility to all servers.
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_get_next_meta(
	PVFS_fs_id fsid,
	bmi_addr_t* meta_addr,
	PVFS_handle* bucket,
	PVFS_handle* handle_mask)
{
	int ret = -1;
	
	/* make sure that they asked for something sane */
	if(fsid != HACK_fsid)
	{
		gossip_lerr("PINT_bucket_get_next_meta() called for invalid fsid.\n");
		return(-EINVAL);
	}

	ret = BMI_addr_lookup(meta_addr, HACK_server_name);
	if(ret < 0)
	{
		return(ret);
	}

	*handle_mask = HACK_handle_mask;
	*bucket = HACK_bucket;

	return(0);
}

/* PINT_bucket_get_next_io()
 *
 * returns the address, bucket, and handle mask of a set of servers that
 * should be used to store new pieces of file data.  This function is
 * responsible for evenly distributing the file data storage load to all
 * servers.
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_get_next_io(
	PVFS_fs_id fsid,
	int num_servers,
	bmi_addr_t* io_addr_array,
	PVFS_handle* bucket_array,
	PVFS_handle* handle_mask)
{
	int i = 0;
	int ret = -1;
	
	/* make sure that they asked for something sane */
	if(fsid != HACK_fsid)
	{
		gossip_lerr("PINT_bucket_get_next_io() called for invalid fsid.\n");
		return(-EINVAL);
	}

	/* NOTE: for now, we assume that if the caller asks for more servers
	 * than we have available, we should just duplicate servers in the
	 * list.  The caller can use get_num_io to find out how many servers
	 * there are if they want to match up.
	 */
	
	ret = BMI_addr_lookup(&(io_addr_array[0]), HACK_server_name);
	if(ret < 0)
	{
		return(ret);
	}

	/* copy the same address to every server position if the user wanted
	 * more than one i/o server
	 */
	for(i=1; i<num_servers; i++)
	{
		io_addr_array[i] = io_addr_array[0];
	}

	/* fill in the same bucket in every slot as well */
	for(i=0; i<num_servers; i++)
	{
		bucket_array[i] = HACK_bucket;
	}

	*handle_mask = HACK_handle_mask;

	return(0);
}


/* PINT_bucket_map_to_server()
 *
 * maps from a bucket and fsid to a server address
 *
 * returns 0 on success to -errno on failure
 */
int PINT_bucket_map_to_server(
	bmi_addr_t* server_addr,
	PVFS_handle bucket,
	PVFS_fs_id fsid)
{	
	int ret = -1;

	/* make sure that they asked for something sane */
	if(fsid != HACK_fsid || bucket != HACK_bucket)
	{
		gossip_lerr("PINT_bucket_map_to_server() called for invalid fsid.\n");
		return(-EINVAL);
	}

	ret = BMI_addr_lookup(server_addr, HACK_server_name);
	if(ret < 0)
	{
		return(ret);
	}

	return(0);
}


/* PINT_bucket_map_from_server()
 *
 * maps from a server to an array of buckets (bounded by inout_count)
 * that it controls.
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_map_from_server(
	char* server_name,
	int* inout_count, 
	PVFS_handle* bucket_array,
	PVFS_handle* handle_mask)
{
	if(strcmp(server_name, HACK_server_name) != 0)
	{
		return(-EINVAL);
	}

	if(*inout_count < 1)
	{
		return(-EINVAL);
	}

	*inout_count = 1;
	bucket_array[0] = HACK_bucket;
	*handle_mask = HACK_handle_mask;

	return(0);
}

/* PINT_bucket_get_num_meta()
 *
 * discovers the number of metadata servers available for a given file
 * system
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_get_num_meta(
	PVFS_fs_id fsid,
	int* num_meta)
{
	
	if(fsid != HACK_fsid)
	{
		return(-EINVAL);
	}

	*num_meta = 1;

	return(0);
}

/* PINT_bucket_get_num_io()
 *
 * discovers the number of io servers available for a given file system
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_get_num_io(
	PVFS_fs_id fsid,
	int* num_io)
{
	
	if(fsid != HACK_fsid)
	{
		return(-EINVAL);
	}

	*num_io = 1;

	return(0);
}

/* PINT_bucket_get_server_name()
 *
 * discovers the string (BMI url) name of a server that controls the
 * specified bucket.
 *
 * returns 0 on success, -errno on failure
 */
int PINT_bucket_get_server_name(
	char* server_name,
	int max_server_name_len,
	PVFS_handle bucket,
	PVFS_fs_id fsid)
{
	
	if(!server_name || bucket != HACK_bucket || fsid != HACK_fsid)
	{
		return(-EINVAL);
	}

	if((strlen(HACK_server_name) + 1) > max_server_name_len)
	{
		return(-EOVERFLOW);
	}

	strcpy(server_name, HACK_server_name);

	return(0);
}

#endif
