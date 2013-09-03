/*
 * (C) 2010-2013 Clemson University and Omnibond Systems, LLC
 *
 * See COPYING in top-level directory.
 */
   
/* 
 * User cache declarations 
 */

#ifndef __USER_CACHE_H
#define __USER_CACHE_H

#include <openssl/asn1.h>

#include "pvfs2.h"
#include "quickhash.h"

struct user_entry
{
    struct qhash_head hash_link;
    char user_name[256];
    PVFS_credential credential;
    ASN1_UTCTIME *expires;
};

int user_compare(void *key, 
                 struct qhash_head *link);

int add_cache_user(char *user_name, 
             PVFS_credential *credential,
             ASN1_UTCTIME *expires);

int get_cache_user(char *user_name, 
                    PVFS_credential *credential);

int remove_user(char *user_name);

unsigned int user_cache_thread(void *options);

#endif