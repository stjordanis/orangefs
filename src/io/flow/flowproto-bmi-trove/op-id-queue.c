/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */

/* used for storing for storing Trove operation id's 
 *
 */

#include <errno.h>
#include <assert.h>
#include <stdlib.h>

#include "op-id-queue.h"
#include "quicklist.h"

struct op_id_entry
{
    int type;
    struct qlist_head queue_link;
    union
    {
	PVFS_ds_id trove_op;
    }
    u;
};

/* op_id_queue_new()
 *
 * create a new queue of operation ids
 *
 * returns pointer to queue on success, NULL on failure
 */
op_id_queue_p op_id_queue_new()
{
    op_id_queue_p tmp_queue = NULL;

    tmp_queue = (op_id_queue_p) malloc(sizeof(struct qlist_head));
    if (tmp_queue)
    {
	INIT_QLIST_HEAD(tmp_queue);
    }

    return (tmp_queue);
}

/* op_id_queue_add()
 *
 * adds a new entry to an op id queue
 *
 * returns 0 on success, -errno on failure
 */
int op_id_queue_add(op_id_queue_p queue,
		    void *id_pointer,
		    int type)
{
    struct op_id_entry *new_entry = NULL;

    new_entry = (struct op_id_entry *) malloc(sizeof(struct op_id_entry));
    if (!new_entry)
    {
	return (-ENOMEM);
    }

    new_entry->type = type;
    switch (type)
    {
    case TROVE_OP_ID:
	new_entry->u.trove_op = *((PVFS_ds_id *) id_pointer);
	break;
    default:
	/* bad type */
	free(new_entry);
	return (-EINVAL);
	break;
    }

    qlist_add_tail(&(new_entry->queue_link), queue);

    return (0);
}

/* op_id_queue_del()
 *
 * deletes an entry from an id queue; no action taken if specified id is
 * not in queue
 *
 * no return value
 */
void op_id_queue_del(op_id_queue_p queue,
		     void *id_pointer,
		     int type)
{
    op_id_queue_p tmp_link = NULL;
    struct op_id_entry *tmp_entry = NULL;

    qlist_for_each(tmp_link, queue)
    {
	tmp_entry = qlist_entry(tmp_link, struct op_id_entry,
				queue_link);
	if (tmp_entry->type == type)
	{
	    if (type == TROVE_OP_ID && 
		tmp_entry->u.trove_op == *((PVFS_ds_id *) id_pointer))
	    {
		qlist_del(tmp_link);
		free(tmp_entry);
		return;
	    }
	}
    }
    return;
}


/* op_id_queue_cleanup()
 *
 * releases an existing queue and any entries stored within it
 *
 * no return value
 */
void op_id_queue_cleanup(op_id_queue_p queue)
{
    op_id_queue_p iterator = NULL;
    op_id_queue_p scratch = NULL;
    struct op_id_entry *tmp_entry;

    qlist_for_each_safe(iterator, scratch, queue)
    {
	tmp_entry = qlist_entry(iterator, struct op_id_entry,
				queue_link);
	free(tmp_entry);
    }

    free(queue);
    queue = NULL;
    return;
}


/* op_id_queue_query()
 *
 * copies the first count ids in the queue into a contiguous array;
 * fills in actual number copies into the count variable
 *
 * returns 0 on success, -errno on failure
 */
int op_id_queue_query(op_id_queue_p queue,
		      void *array,
		      int *count,
		      int type)
{
    PVFS_ds_id *trove_array = array;
    op_id_queue_p tmp_link = NULL;
    struct op_id_entry *tmp_entry = NULL;
    int current_index = 0;

    assert(type == TROVE_OP_ID);

    if (type == TROVE_OP_ID)
    {
	qlist_for_each(tmp_link, queue)
	{
	    tmp_entry = qlist_entry(tmp_link, struct op_id_entry,
				    queue_link);
	    if (tmp_entry->type == type)
	    {
		trove_array[current_index] = tmp_entry->u.trove_op;
		current_index++;
		if (current_index == *count)
		{
		    return (0);
		}
	    }
	}
    }

    *count = current_index;
    return (0);
}

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 noexpandtab
 */
