/*
 * Copyright 2015 Omnibond Systems, L.L.C.
 *
 * See COPYING in top-level directory.
 */

#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <gossip.h>

#include <lmdb.h>

#include "dbpf.h"

struct dbpf_db {
    MDB_env *env;
    MDB_dbi dbi;
};

struct dbpf_cursor {
    MDB_cursor *cursor;
    MDB_txn *txn;
};

static int db_error(int e)
{
    /* values greater than zero are errno values */
    if (e > 0)
    {
        /* XXX: no need to pass return of this function into
         * here as almost all of the code does. */
        return trove_errno_to_trove_error(e);
    }
    else if (!e)
    {
        return 0;
    }
    switch (e)
    {
    case MDB_NOTFOUND:
        return TROVE_ENOENT;
    }
    return DBPF_ERROR_UNKNOWN;
}

static int ds_attr_compare(const MDB_val *a, const MDB_val *b)
{
    TROVE_handle handle_a, handle_b;
    int cmpval;
    if (a->mv_size < sizeof handle_a ||
        b->mv_size < sizeof handle_b)
    {
        gossip_err("inconsistency in ds_attr_compare: size too small\n");
        abort();
    }
    memcpy(&handle_a, a->mv_data, sizeof handle_a);
    memcpy(&handle_b, b->mv_data, sizeof handle_b);

    if(!(cmpval = PVFS_OID_cmp(&handle_a, &handle_b)))
    {
        return 0;
    }
    return (cmpval > 0) ? -1 : 1;
}

static int keyval_compare(const MDB_val *a, const MDB_val *b)
{
    struct dbpf_keyval_db_entry db_entry_a, db_entry_b;
    int cmpval;
    if (sizeof db_entry_a < a->mv_size ||
        sizeof db_entry_b < b->mv_size)
    {
        gossip_err("inconsistency in keyval_compare: size too small\n");
        abort();
    }
    memcpy(&db_entry_a, a->mv_data, a->mv_size);
    memcpy(&db_entry_b, b->mv_data, b->mv_size);

    if((cmpval = PVFS_OID_cmp(&db_entry_a.handle, &db_entry_b.handle)))
    {
        return (cmpval < 0) ? -1 : 1;
    }
    if (db_entry_a.type != db_entry_b.type)
    {
        return (db_entry_a.type < db_entry_b.type) ? -1 : 1;
    }
    if (a->mv_size > b->mv_size)
    {
        return 1;
    }
    else if (a->mv_size < b->mv_size)
    {
        return -1;
    }
    /* else must be equal */
    return (memcmp(db_entry_a.key, db_entry_b.key,
        DBPF_KEYVAL_DB_ENTRY_KEY_SIZE(a->mv_size)));
}

int dbpf_db_open(char *name, int compare, struct dbpf_db **db,
    int create)
{
    MDB_txn *txn;
    int r;

    *db = malloc(sizeof **db);
    if (!*db)
    {
        return db_error(errno);
    }

    r = mdb_env_create(&(*db)->env);
    if (r)
    {
        mdb_env_close((*db)->env);
        free(*db);
        return db_error(r);
    }

    if (create)
    {
        r = mkdir(name, TROVE_DB_MODE|S_IXUSR|S_IXGRP|S_IXOTH);
        if (r)
        {
            mdb_env_close((*db)->env);
            free(*db);
            return db_error(errno);
        }
    }
    r = mdb_env_open((*db)->env, name, MDB_NOSYNC, TROVE_DB_MODE);
    if (r)
    {
        mdb_env_close((*db)->env);
        free(*db);
        return db_error(r);
    }

    r = mdb_txn_begin((*db)->env, NULL, MDB_RDONLY, &txn);
    if (r)
    {
        mdb_env_close((*db)->env);
        free(*db);
        return db_error(r);
    }

    r = mdb_dbi_open(txn, NULL, create ? MDB_CREATE : 0, &(*db)->dbi);
    if (r)
    {
        mdb_txn_abort(txn);
        mdb_env_close((*db)->env);
        free(*db);
        return db_error(r);
    }

    if (compare == DBPF_DB_COMPARE_DS_ATTR)
    {
        r = mdb_set_compare(txn, (*db)->dbi, ds_attr_compare);
    }
    else if (compare == DBPF_DB_COMPARE_KEYVAL)
    {
        r = mdb_set_compare(txn, (*db)->dbi, keyval_compare);
    }
    if (r)
    {
        mdb_txn_abort(txn);
        mdb_env_close((*db)->env);
        free(*db);
        return db_error(r);
    }

    r = mdb_txn_commit(txn);
    if (r)
    {
        mdb_env_close((*db)->env);
        free(*db);
        return db_error(errno);
    }

    return 0;
}

int dbpf_db_close(struct dbpf_db *db)
{
    mdb_env_close(db->env);
    free(db);
    return 0;
}

int dbpf_db_sync(struct dbpf_db *db)
{
    return db_error(mdb_env_sync(db->env, 0));
}

int dbpf_db_get(struct dbpf_db *db, struct dbpf_data *key,
    struct dbpf_data *val)
{
    MDB_val db_key, db_data;
    MDB_txn *txn;
    int r;

    db_key.mv_size = key->len;
    db_key.mv_data = key->data;

    r = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
    if (r)
    {
        return db_error(r);
    }
    r = mdb_get(txn, db->dbi, &db_key, &db_data);
    if (r)
    {
        mdb_txn_abort(txn);
        return db_error(r);
    }
    r = mdb_txn_commit(txn);
    if (r)
    {
        return db_error(r);
    }

    if (db_data.mv_size > val->len)
    {
        return db_error(ERANGE);
    }
    memcpy(val->data, db_data.mv_data, db_data.mv_size);
    val->len = db_data.mv_size;
    return 0;
}

int dbpf_db_put(struct dbpf_db *db, struct dbpf_data *key,
    struct dbpf_data *val)
{
    MDB_val db_key, db_data;
    MDB_txn *txn;
    int r;

    db_key.mv_size = key->len;
    db_key.mv_data = key->data;
    db_data.mv_size = val->len;
    db_data.mv_data = val->data;

    r = mdb_txn_begin(db->env, NULL, 0, &txn);
    if (r)
    {
        return db_error(r);
    }
    r = mdb_put(txn, db->dbi, &db_key, &db_data, 0);
    if (r)
    {
        mdb_txn_abort(txn);
        return db_error(r);
    }
    r = mdb_txn_commit(txn);
    if (r)
    {
        return db_error(r);
    }
    return 0;
}

int dbpf_db_putonce(struct dbpf_db *db, struct dbpf_data *key,
    struct dbpf_data *val)
{
    MDB_val db_key, db_data;
    MDB_txn *txn;
    int r;

    db_key.mv_size = key->len;
    db_key.mv_data = key->data;
    db_data.mv_size = val->len;
    db_data.mv_data = val->data;

    r = mdb_txn_begin(db->env, NULL, 0, &txn);
    if (r)
    {
        return db_error(r);
    }
    r = mdb_put(txn, db->dbi, &db_key, &db_data, MDB_NOOVERWRITE);
    if (r)
    {
        mdb_txn_abort(txn);
        return db_error(r);
    }
    r = mdb_txn_commit(txn);
    if (r)
    {
        return db_error(r);
    }
    return 0;
}

int dbpf_db_del(struct dbpf_db *db, struct dbpf_data *key)
{
    MDB_val db_key;
    MDB_txn *txn;
    int r;

    db_key.mv_size = key->len;
    db_key.mv_data = key->data;

    r = mdb_txn_begin(db->env, NULL, 0, &txn);
    if (r)
    {
        return db_error(r);
    }
    r = mdb_del(txn, db->dbi, &db_key, NULL);
    if (r)
    {
        mdb_txn_abort(txn);
        return db_error(r);
    }
    r = mdb_txn_commit(txn);
    if (r)
    {
        return db_error(r);
    }
    return 0;
}

int dbpf_db_cursor(struct dbpf_db *db, struct dbpf_cursor **dbc, int rdonly)
{
    int r;
    *dbc = malloc(sizeof **dbc);
    if (!*dbc)
    {
        return db_error(errno);
    }

    r = mdb_txn_begin(db->env, NULL, rdonly ? MDB_RDONLY : 0, &(*dbc)->txn);
    if (r)
    {
        free(*dbc);
        return db_error(r);
    }
    r = mdb_cursor_open((*dbc)->txn, db->dbi, &(*dbc)->cursor);
    if (r)
    {
        mdb_txn_abort((*dbc)->txn);
        free(*dbc);
        return db_error(r);
    }
    return 0;
}

int dbpf_db_cursor_close(struct dbpf_cursor *dbc)
{
    int r;
    mdb_cursor_close(dbc->cursor);
    r = mdb_txn_commit(dbc->txn);
    if (r)
    {
        free(dbc);
        return db_error(r);
    }
    free(dbc);
    return 0;
}

int dbpf_db_cursor_get(struct dbpf_cursor *dbc, struct dbpf_data *key,
    struct dbpf_data *val, int op, size_t maxkeylen)
{
    MDB_val db_key, db_data;
    int db_op, r;
    switch (op) {
    case DBPF_DB_CURSOR_NEXT:
        db_op = MDB_NEXT;
        break;
    case DBPF_DB_CURSOR_CURRENT:
        db_op = MDB_GET_CURRENT;
        break;
    case DBPF_DB_CURSOR_SET:
        db_key.mv_size = key->len;
        db_key.mv_data = key->data;
        db_op = MDB_SET_KEY;
        break;
    case DBPF_DB_CURSOR_SET_RANGE:
        db_key.mv_size = key->len;
        db_key.mv_data = key->data;
        db_op = MDB_SET_RANGE;
        break;
    case DBPF_DB_CURSOR_FIRST:
        db_op = MDB_FIRST;
        break;
    }
    r = mdb_cursor_get(dbc->cursor, &db_key, &db_data, db_op);
    if (r)
    {
        return db_error(r);
    }

    if (db_key.mv_size > maxkeylen)
    {
        return db_error(ERANGE);
    }
    memcpy(key->data, db_key.mv_data, db_key.mv_size);
    key->len = db_key.mv_size;
    if (db_data.mv_size > val->len)
    {
        return db_error(ERANGE);
    }
    memcpy(val->data, db_data.mv_data, db_data.mv_size);
    val->len = db_data.mv_size;
    return 0;
}

int dbpf_db_cursor_del(struct dbpf_cursor *dbc)
{
    return db_error(mdb_cursor_del(dbc->cursor, 0));
}
