/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */

#include <client.h>
#include "helper.h"

#define MAX_PATHNAME_LEN 256

int main(int argc,char **argv)
{
    int ret = -1;
    char old_buf[MAX_PATHNAME_LEN] = {0};
    char new_buf[MAX_PATHNAME_LEN] = {0};
    char *old_filename = (char *)0;
    char *new_filename = (char *)0;
    PVFS_fs_id cur_fs;
    pvfs_mntlist mnt = {0,NULL};
    PVFS_sysresp_init resp_init;
    PVFS_sysreq_remove req_remove;

    gossip_enable_stderr();
    gossip_set_debug_mask(1,CLIENT_DEBUG);

    if (argc != 3)
    {
        printf("usage: %s old_pathname new_pathname\n", argv[0]);
        return 1;
    }
    old_filename = argv[1];
    new_filename = argv[2];

    if (parse_pvfstab(NULL,&mnt))
    {
        printf("Failed to parse pvfstab\n");
        return ret;
    }

    memset(&resp_init, 0, sizeof(resp_init));
    if (PVFS_sys_initialize(mnt, &resp_init))
    {
        printf("Failed to initialize system interface\n");
        return ret;
    }

    if (PINT_remove_base_dir(old_filename, old_buf, MAX_PATHNAME_LEN))
    {
        if (old_filename[0] != '/')
        {
            printf("You forgot the leading '/'\n");
        }
        printf("Cannot retrieve entry name for %s\n",
               old_filename);
        return(-1);
    }
    printf("Old filename is %s\n", old_buf);

    if (PINT_remove_base_dir(new_filename, new_buf, MAX_PATHNAME_LEN))
    {
        if (new_filename[0] != '/')
        {
            printf("You forgot the leading '/'\n");
        }
        printf("Cannot retrieve name %s\n",
               new_filename);
        return(-1);
    }
    printf("New filename is %s\n",new_buf);

    memset(&req_rename,0,sizeof(PVFS_sysreq_rename));

    cur_fs = resp_init.fsid_list[0];

    req_rename.old_entry = old_buf;
    req_rename.old_parent_refn.handle =
        lookup_parent_handle(old_filename,cur_fs);
    req_rename.old_parent_refn.fs_id = cur_fs;
    req_rename.new_entry = new_buf;
    req_rename.new_parent_refn.handle =
        lookup_parent_handle(old_filename,cur_fs);
    req_rename.new_parent_refn.fs_id = cur_fs;
    req_rename.credentials.uid = 100;
    req_rename.credentials.gid = 100;
    req_rename.credentials.perms = 1877;

    ret = PVFS_sys_rename(&req_rename);
    if (ret < 0)
    {
        printf("rename failed with errcode = %d\n",ret);
        return(-1);
    }

    printf("===================================");
    printf("file named %s has been renamed to %s.", old_filename,  new_filename);

    //close it down
    ret = PVFS_sys_finalize();
    if (ret < 0)
    {
        printf("finalizing sysint failed with errcode = %d\n", ret);
        return (-1);
    }

    gossip_disable();

    return(0);
}
