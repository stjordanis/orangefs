/*
 * (C) 2010-2013 Clemson University and Omnibond Systems, LLC
 *
 * See COPYING in top-level directory.
 */

/* 
 * LDAP support declarations 
 */

#ifndef __LDAP_SUPPORT_H
#define __LDAP_SUPPORT_H

#include "pvfs2.h"
#include "client-service.h"

int PVFS_ldap_init();

void PVFS_ldap_cleanup();

int get_ldap_credential(char *userid,
                         PVFS_credential *credential);

#endif