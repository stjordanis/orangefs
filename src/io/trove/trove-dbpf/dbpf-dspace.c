/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <db.h>
#include <time.h>
#include <malloc.h>

#include <trove.h>
#include <trove-internal.h>
#include <dbpf.h>
#include <dbpf-op-queue.h>
#include <id-generator.h>
#include <trove-ledger.h>

/* TODO: move both of these into header file? */
extern struct dbpf_collection *my_coll_p;
extern TROVE_handle dbpf_last_handle;

static int dbpf_dspace_iterate_handles_op_svc(struct dbpf_op *op_p);

/* TODO: should this have a ds_attributes with it? */
int dbpf_dspace_create(TROVE_coll_id coll_id,
			      TROVE_handle *handle_p,
			      TROVE_handle bitmask,
			      TROVE_ds_type type,
			      TROVE_keyval_s *hint, /* TODO: What is this? */
			      void *user_ptr,
			      TROVE_op_id *out_op_id_p)
{
    int ret;
    struct dbpf_collection *coll_p;
    struct dbpf_dspace_attr attr;
    TROVE_handle new_handle;
    DBT key, data;
    
    /* TODO: search for collection using coll_id */
    coll_p = my_coll_p;
    if (coll_p == NULL) return -1;

#if 0
    /* need a better handle generation routine than this... */
    /* a free list would make a certain amount of sense; perhaps
     * with ranges?
     */
    new_handle = (*handle_p & bitmask) | (dbpf_last_handle & (~bitmask));
#endif
    /* TODO: REDO BUCKETS!!!! */
    new_handle = trove_handle_get(coll_p->free_handles, *handle_p, bitmask);
    
    printf("new handle = %Lu (%Lx).\n", new_handle, new_handle);
    
    attr.coll_id = coll_id;
    attr.type = type;
    memset(&key, 0, sizeof(key));
    memset(&data, 0, sizeof(data));
    key.data = &new_handle;
    key.size = sizeof(new_handle);
    
    /* check to see if handle is already used */
    ret = coll_p->ds_db->get(coll_p->ds_db, NULL, &key, &data, 0);
    if (ret == 0) {
	printf("handle already exists...\n");
	return -1;
    }
    if (ret != DB_NOTFOUND) {
	printf("some other error in dspace create.\n");
	return -1;
    }
    
    memset(&data, 0, sizeof(data));
    data.data = &attr;
    data.size = sizeof(attr);
    
    /* create new dataspace entry */
    ret = coll_p->ds_db->put(coll_p->ds_db, NULL, &key, &data, 0);
    if (ret != 0) {
	return -1;
    }
    
    /* always sync to ensure that data made it to the disk */
    if ((ret = coll_p->ds_db->sync(coll_p->ds_db, 0)) != 0) {
	return -1;
    }
    
    printf("created new dspace with above handle.\n");
    
    *handle_p = new_handle;
    dbpf_last_handle++;
    return 1;
}

/* dbpf_dspace_remove()
 * XXX: maybe make this an op_svc()?
 */
static int dbpf_dspace_remove(TROVE_coll_id coll_id,
			      TROVE_handle handle,
			      void *user_ptr,
			      TROVE_op_id *out_op_id_p)
{
	int ret;
	struct dbpf_collection *coll_p;
	DBT key;

	/* TODO: search for collection using coll_id */
	coll_p = my_coll_p;
	if (coll_p == NULL) return -1;

	/* whereas dspace_create has to do handle-making steps, we already know
	 * the handle we want to wack */

	memset(&key, 0, sizeof(key));
	key.data = &handle;
	key.size = sizeof(handle);

	/* XXX: no steps taken to ensure it's empty... */
	ret = coll_p->ds_db->del(coll_p->ds_db, NULL, &key, 0);
	switch (ret) {
		case 0:
			printf("removed dataspace with handle %Ld\n", handle);
			break;
		case DB_NOTFOUND:
			printf("tried to remove non-existant dataspace\n");
			return -1;
			break;
		default:
			coll_p->ds_db->err(coll_p->ds_db, ret, "dbpf_dspace_remove");
			return -1;
	}

	/* always sync to ensure that data made it to the disk */
	if ( (ret = coll_p->ds_db->sync(coll_p->ds_db, 0)) != 0) {
		coll_p->ds_db->err(coll_p->ds_db, ret, "dbpf_dspace_remove");
		return -1;
	}

	/* return handle to free list */
	trove_handle_put(coll_p->free_handles, handle);

	return 1;

}
int dbpf_dspace_iterate_handles(
                                TROVE_coll_id coll_id,
                                TROVE_ds_position *position_p,
                                TROVE_handle *handle_array,
                                int *inout_count_p,
                                TROVE_ds_flags flags,
                                TROVE_vtag_s *vtag,
                                void *user_ptr,
                                TROVE_op_id *out_op_id_p)
{
    TROVE_op_id new_id;
    struct dbpf_queued_op *q_op_p;
    struct dbpf_collection *coll_p;

    coll_p = my_coll_p;
    if (coll_p == NULL) return -1;

    q_op_p = dbpf_queued_op_alloc();
    if (q_op_p == NULL) return -1;

    id_gen_fast_register(&new_id, q_op_p);

    /* initialize all the common members */
    dbpf_queued_op_init(q_op_p,
			KEYVAL_ITERATE,
			0, /* handle -- ignored in this case */
			new_id,
			coll_p,
			dbpf_dspace_iterate_handles_op_svc,
			user_ptr);

    /* initialize op-specific members */
    q_op_p->op.u.d_iterate_handles.handle_array = handle_array;
    q_op_p->op.u.d_iterate_handles.position_p   = position_p;
    q_op_p->op.u.d_iterate_handles.count_p      = inout_count_p;

    dbpf_queued_op_queue(q_op_p);
    *out_op_id_p = new_id;

    return 0;
}

/* dbpf_dspace_iterate_handles_op_svc()
 */
static int dbpf_dspace_iterate_handles_op_svc(struct dbpf_op *op_p)
{
    int ret, i=0;
    DB *db_p;
    DBC *dbc_p;
    DBT key, data;
    db_recno_t recno;
    struct dbpf_dspace_attr attr;

    db_p = op_p->coll_p->ds_db;
    if (db_p == NULL) goto return_error;

    /* grab out key/value pairs */

    /* get a cursor */
    ret = db_p->cursor(db_p, NULL, &dbc_p, 0);
    if (ret != 0) goto return_error;

    /* we have two choices here: 'seek' to a specific key by either a:
     * specifying a key or b: using record numbers in the db.  record numbers
     * will serialize multiple modification requests. We are going with record
     * numbers for now. i don't know if that's a problem, but it is something
     * to keep in mind if at some point simultaneous modifications to pvfs
     * perform badly.   -- robl
     *
     * not sure this is the best thing to be doing, but ok for now... -- rob
     */

    if (*op_p->u.d_iterate_handles.position_p == SI_START_POSITION) {
	/* an uninitialized cursor will start reading at the beginning
	 * of the database (first record) when used with DB_NEXT, so
	 * we don't need to position with DB_FIRST.
	 */
	printf("si_start_pos detected\n");

#if 0
	memset(&key, 0, sizeof(key));
	key.data = &op_p->u.d_iterate_handles.handle_array[0];
	key.size = key.ulen = sizeof(TROVE_handle);
	key.flags |= DB_DBT_USERMEM;

	memset(&data, 0, sizeof(data));
	data.data = &attr;
	data.size = data.ulen = sizeof(attr);
	data.flags |= DB_DBT_USERMEM;

	ret = dbc_p->c_get(dbc_p, &key, &data, DB_FIRST);
	if (ret == DB_NOTFOUND) goto return_ok;
	else if (ret != 0) {
	    printf("c_get failed on iteration %d\n", i);
	    goto return_error;
	}
	i++;
#endif
    }
    else {
	/* we need to position the cursor before we can read new entries.
	 * we will go ahead and read the first entry as well, so that we
	 * can use the same loop below to read the remainder in this or
	 * the above case.
	 */

	/* set position */
	recno = *op_p->u.d_iterate_handles.position_p;
	memset(&key, 0, sizeof(key));
	key.data  = &recno;
	key.size  = key.ulen = sizeof(recno);
	key.flags |= DB_DBT_USERMEM;

	memset(&data, 0, sizeof(data));
	data.data = &attr;
	data.size = data.ulen = sizeof(attr);
	data.flags |= DB_DBT_USERMEM;

	ret = dbc_p->c_get(dbc_p, &key, &data, DB_SET_RECNO);
	if (ret == DB_NOTFOUND) goto return_ok;
	if (ret != 0) goto return_error;

	/* read current to get current key */
	memset(&key, 0, sizeof(key));
	key.data  = &op_p->u.d_iterate_handles.handle_array[0];
	key.size  = key.ulen = sizeof(TROVE_handle);
	key.flags |= DB_DBT_USERMEM;

	memset(&data, 0, sizeof(data));
	data.data = &attr;
	data.size = data.ulen = sizeof(attr);
	data.flags |= DB_DBT_USERMEM;

	ret = dbc_p->c_get(dbc_p, &key, &data, DB_CURRENT);
	if (ret == DB_NOTFOUND) goto return_ok;
	if (ret != 0) goto return_error;
    }

    /* read more handles until we run out of handles or space in buffer */
    for (/* i init'd at top of fn */; i < *op_p->u.d_iterate_handles.count_p; i++) {
	memset(&key, 0, sizeof(key));
	key.data = &op_p->u.d_iterate_handles.handle_array[i];
	key.size = key.ulen = sizeof(TROVE_handle);
	key.flags |= DB_DBT_USERMEM;

	memset(&data, 0, sizeof(data));
	data.data = &attr;
	data.size = data.ulen = sizeof(attr);
	data.flags |= DB_DBT_USERMEM;
	    
	ret = dbc_p->c_get(dbc_p, &key, &data, DB_NEXT);
	if (ret == DB_NOTFOUND) goto return_ok;
	else if (ret != 0) {
	    printf("c_get failed on iteration %d\n", i);
	    goto return_error;
	}
    }

    /* get the record number to return.
     *
     * note: key field is ignored by c_get in this case
     */
    memset(&data, 0, sizeof(data));
    data.data = &recno;
    data.size = data.ulen = sizeof(recno);
    data.flags |= DB_DBT_USERMEM;

    ret = dbc_p->c_get(dbc_p, NULL, &data, DB_GET_RECNO);

    *(op_p->u.d_iterate_handles.position_p) = recno;

    /* 'position' is the record we will read next time through */
    /* XXX: right now, 'posistion' gets set to garbage when we hit the end.
     * well, not exactly 'garbage', but it gets incremented here even in the
     * end-of-database case ( come in, ask for one, read zero (hit end):
     * position still gets incremented ).  It might be helpful (or at least
     * consistent) if posistion always pointed to the 'next' place to access,
     * even if that's one place past the end of the database ... or maybe i'm
     * putting too much weight on 'posistion's value at the end */

return_ok:
    *op_p->u.d_iterate_handles.count_p = i;
    op_p->state = OP_COMPLETED;

    ret = dbc_p->c_close(dbc_p);
    if (ret != 0) return -1;
    return 1;

return_error:
    fprintf(stderr, "dbpf_dspace_iterate_handles_op_svc: %s\n", db_strerror(ret));
    *op_p->u.d_iterate_handles.count_p = i; 
    op_p->state = OP_COMPLETED;
    dbc_p->c_close(dbc_p); /* don't check error -- we're returning an error anyway. */

    return -1;
}

/* dbpf_dspace_verify()
 */
static int dbpf_dspace_verify(TROVE_coll_id coll_id,
			      TROVE_handle handle,
			      TROVE_ds_type *type,
			      void *user_ptr,
			      TROVE_op_id *out_op_id_p)
{
    return -1;
}

/* dbpf_dspace_getattr()
 */
static int dbpf_dspace_getattr(TROVE_coll_id coll_id,
			       TROVE_handle handle,
			       TROVE_ds_attributes_s *ds_attr_p, 
			       void *user_ptr,
			       TROVE_op_id *out_op_id_p)
{
    return -1;
}

/* dbpf_dspace_setattr()
 */
static int dbpf_dspace_setattr(TROVE_coll_id coll_id,
			       TROVE_handle handle,
			       TROVE_ds_attributes_s *ds_attr_p, 
			       void *user_ptr,
			       TROVE_op_id *out_op_id_p)
{
    return -1;
}

/* dbpf_dspace_test()
 *
 * Here's how this works:
 * - Caller gives us an id (we don't really need the coll_id)
 * - Map to queued operation with id_gen_fast_lookup(id)
 * - Call service function that is stored in operation structure
 * ...
 *
 * Returns 0 if not completed, 1 if completed (successfully or with error).
 *
 * The error state of the completed operation is returned via the state_p,
 * more to follow on this...
 *
 * Removes completed operations from the queue.
 */
static int dbpf_dspace_test(TROVE_coll_id coll_id,
			    TROVE_op_id id,
			    int *out_count_p,
			    TROVE_vtag_s *vtag,
			    void **returned_user_ptr_p,
			    TROVE_ds_state *state_p)
{
    int ret;
    struct dbpf_queued_op *q_op_p;
    
    /* map to queued operation */
    q_op_p = id_gen_fast_lookup(id);
    
    /* for now we need to call the service function;
     * there's no thread to do the work yet.
     */
    ret = q_op_p->op.svc_fn(&(q_op_p->op));

    if (ret != 0) {
	/* operation is done and we are telling the caller;
	 * ok to pull off queue now.
	 */
	*out_count_p = 1;
	*state_p = (ret == 1) ? 0 : -1; /* TODO: FIX THIS!!! */
	if (returned_user_ptr_p != NULL) {
	    *returned_user_ptr_p = q_op_p->op.user_ptr;
	}
	dbpf_queued_op_dequeue(q_op_p);
	return 1;
    }
	
    return 0;
}

/* dbpf_dspace_testsome()
 *
 * Returns 0 if nothing completed, 1 if something is completed (successfully
 * or with error).
 *
 * The error state of the completed operation is returned via the state_p,
 * more to follow on this...
 *
 */
static int dbpf_dspace_testsome(
				TROVE_coll_id coll_id,
				TROVE_op_id *ds_id_array,
				int *inout_count_p,
				int *out_index_array,
				TROVE_vtag_s *vtag_array,
				void **returned_user_ptr_array,
				TROVE_ds_state *state_array
				)
{
    int i, out_count = 0, ret;
    struct dbpf_queued_op *q_op_p;

    for (i=0; i < *inout_count_p; i++) {
	/* map to queued operation */
	q_op_p = id_gen_fast_lookup(ds_id_array[i]);
	
	/* for now we need to call the service function;
	 * there's no thread to do the work yet.
	 */
	ret = q_op_p->op.svc_fn(&(q_op_p->op));
	
	if (ret != 0) {
	    /* operation is done and we are telling the caller;
	     * ok to pull off queue now.
	     */
	    out_index_array[out_count] = i;
	    state_array[out_count] = (ret == 1) ? 0 : -1; /* TODO: FIX THIS!!! */
	    if (returned_user_ptr_array != NULL) {
		returned_user_ptr_array[out_count] = q_op_p->op.user_ptr;
	    }
	    out_count++;
	    dbpf_queued_op_dequeue(q_op_p);
	}    
    }

    *inout_count_p = out_count;
    return (out_count > 0) ? 1 : 0;
}

struct TROVE_dspace_ops dbpf_dspace_ops =
{
    dbpf_dspace_create,
    dbpf_dspace_remove,
    dbpf_dspace_iterate_handles,
    dbpf_dspace_verify,
    dbpf_dspace_getattr,
    dbpf_dspace_setattr,
    dbpf_dspace_test,
    dbpf_dspace_testsome
};

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 */
