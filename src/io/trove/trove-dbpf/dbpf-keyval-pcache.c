/*
 * (C) 2002 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include "dbpf-keyval-pcache.h"
#include "quickhash.h"
#include "gossip.h"
#include "pvfs2-internal.h"

/* table size must be a multiple of 2 */
#define DBPF_KEYVAL_PCACHE_TABLE_SIZE (1<<10)
#define DBPF_KEYVAL_PCACHE_HARD_LIMIT 51200

struct dbpf_keyval_pcache_key
{
    TROVE_handle handle;
//    char type;
    TROVE_ds_position pos;
};

struct dbpf_keyval_pcache_entry
{
    TROVE_handle handle;
    TROVE_ds_position pos;
    char keyname[PVFS_NAME_MAX];
    int keylen;
};

static int dbpf_keyval_pcache_compare(
    const void * key, struct qhash_head * link);
static int dbpf_keyval_pcache_hash(
    const void * key, int size);
static int dbpf_keyval_pcache_entry_free(
    void * entry);

PINT_dbpf_keyval_pcache * PINT_dbpf_keyval_pcache_initialize(void)
{
    PINT_dbpf_keyval_pcache * cache;

gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG, "dbpf_keyval_pcache_initialize");
    cache = malloc(sizeof(PINT_dbpf_keyval_pcache));
    if(!cache)
    {
        return NULL;
    }

    gen_mutex_init(&cache->mutex);

    cache->tcache = PINT_tcache_initialize(
        dbpf_keyval_pcache_compare,
        dbpf_keyval_pcache_hash,
        dbpf_keyval_pcache_entry_free,
        DBPF_KEYVAL_PCACHE_TABLE_SIZE);
    if(!cache->tcache)
    {
        return NULL;
    }

    PINT_tcache_set_info(cache->tcache,
                         TCACHE_ENABLE_EXPIRATION,
                         0);
    PINT_tcache_set_info(cache->tcache,
                         TCACHE_HARD_LIMIT,
                         DBPF_KEYVAL_PCACHE_HARD_LIMIT);


    return cache;
}
    
void PINT_dbpf_keyval_pcache_finalize(
    PINT_dbpf_keyval_pcache * cache)
{
gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG, "dbpf_keyval_pcache_finalize");
    PINT_tcache_finalize(cache->tcache);
    gen_mutex_destroy(&cache->mutex);
    free(cache);
}

static int dbpf_keyval_pcache_compare(const void * key,
                                      struct qhash_head * link)
{
    const struct dbpf_keyval_pcache_key * key_entry = 
                    (const struct dbpf_keyval_pcache_key *)key;
    struct dbpf_keyval_pcache_entry * link_entry =
             (struct dbpf_keyval_pcache_entry *)
             (qhash_entry(link, struct PINT_tcache_entry, hash_link))->payload;

    if(!PVFS_OID_cmp(&key_entry->handle, &link_entry->handle) &&
            key_entry->pos == link_entry->pos) 
    {
        gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG,
                     "dbpf_keyval_pcache_compare: "
                     "looking for handle: %s, pos: %llu, "
                     "examining handle: %s, pos: %llu\n",
                     PVFS_OID_str(&key_entry->handle), llu(key_entry->pos),
                     PVFS_OID_str(&link_entry->handle), llu(link_entry->pos));

        return 1;
    }
    return 0;
}
    
/* hash from http://burtleburtle.net/bob/hash/evahash.html */
#define mix(a,b,c) \
do { \
      a=a-b;  a=a-c;  a=a^(c>>13); \
      b=b-c;  b=b-a;  b=b^(a<<8);  \
      c=c-a;  c=c-b;  c=c^(b>>13); \
      a=a-b;  a=a-c;  a=a^(c>>12); \
      b=b-c;  b=b-a;  b=b^(a<<16); \
      c=c-a;  c=c-b;  c=c^(b>>5);  \
      a=a-b;  a=a-c;  a=a^(c>>3);  \
      b=b-c;  b=b-a;  b=b^(a<<10); \
      c=c-a;  c=c-b;  c=c^(b>>15); \
} while(0)

static int dbpf_keyval_pcache_hash(const void * key, int size)
{
    const struct dbpf_keyval_pcache_entry * key_entry =
                    (const struct dbpf_keyval_pcache_entry *)key;

    uint64_t handle = PVFS_OID_hash64(&key_entry->handle);

    uint32_t a = (uint32_t)(handle >> 32);
    uint32_t b = (uint32_t)(handle & 0x00000000FFFFFFFF);
    uint32_t c = (uint32_t)(key_entry->pos);

    mix(a,b,c);
    return (int)(c & (DBPF_KEYVAL_PCACHE_TABLE_SIZE-1));
}

static int dbpf_keyval_pcache_entry_free(void * entry)
{
gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG, "dbpf_keyval_pcache_entry_free");
    free(entry);
    return 0;
}

int PINT_dbpf_keyval_pcache_lookup(
    PINT_dbpf_keyval_pcache *pcache,
    TROVE_handle handle,
    TROVE_ds_position pos,
    const void ** keyname,
    int * length)
{
    struct PINT_tcache_entry *entry;
    struct dbpf_keyval_pcache_key key;
    int ret, status;

    key.handle = handle;
    key.pos = pos;
    
    gen_mutex_lock(&pcache->mutex);
    ret = PINT_tcache_lookup(pcache->tcache, (void *)&key, &entry, &status);
    if(ret != 0)
    {
        if(ret == -PVFS_ENOENT)
        {
            gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG,
                         "Trove KeyVal pcache NOTFOUND: "
                         "handle: %s, pos: %llu\n",
                         PVFS_OID_str(&handle), llu(pos));
        }
        else
        {
            gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG,
                         "Trove KeyVal pcache failed: (error %d): "
                         "handle: %s, pos: %llu\n",
                         ret, PVFS_OID_str(&handle), llu(pos));
        }

        gen_mutex_unlock(&pcache->mutex);
        return ret;
    }
    gen_mutex_unlock(&pcache->mutex);

    *keyname = ((struct dbpf_keyval_pcache_entry *)entry->payload)->keyname;
    *length = ((struct dbpf_keyval_pcache_entry *)entry->payload)->keylen;

    gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG,
                 "Trove KeyVal pcache lookup succeeded: "
                 "handle: %s, pos: %llu\n",
                 PVFS_OID_str(&handle), llu(pos));

    return 0;
}

int PINT_dbpf_keyval_pcache_insert( 
    PINT_dbpf_keyval_pcache *pcache,
    TROVE_handle handle,
//    char type,
    TROVE_ds_position pos,
    const char * keyname,
    int length)
{
    struct dbpf_keyval_pcache_entry *entry;
    struct dbpf_keyval_pcache_key key;
    struct PINT_tcache_entry * tentry;
    int lookup_status;
    int ret;
    int removed;
    
    entry = malloc(sizeof(struct dbpf_keyval_pcache_entry));
    if(!entry)
    {
        return -PVFS_ENOMEM;
    }

    key.handle = handle;
//    key.type = type;
    key.pos = pos;

    gen_mutex_lock(&pcache->mutex);
    if(PINT_tcache_lookup(
            pcache->tcache, (void *)&key, &tentry, &lookup_status) == 0)
    {
        /* remove entry that already exists */
        PINT_tcache_delete(pcache->tcache, tentry);
    }

    entry->handle = handle;
    entry->pos = pos;
    memcpy(entry->keyname, keyname, length);
    entry->keylen = length;

    ret = PINT_tcache_insert_entry(pcache->tcache,
                                   &key,
                                   entry,
                                   &removed);
    if(ret != 0)
    {
        gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG,
                     "Trove KeyVal pcache insert failed: (error: %d) "
                     "handle: %s, pos: %llu\n",
                     ret, PVFS_OID_str(&handle), llu(pos));

        gen_mutex_unlock(&pcache->mutex);
        free(entry);
        return ret;
    }
    gen_mutex_unlock(&pcache->mutex);

    gossip_debug(GOSSIP_DBPF_KEYVAL_DEBUG,
                 "Trove KeyVal pcache insert succeeded: "
                 "handle: %s, pos: %llu\n",
                 PVFS_OID_str(&handle), llu(pos));

    return 0;
}
    
/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
