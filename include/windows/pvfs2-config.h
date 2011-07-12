/* pvfs2-config.h.  Generated from pvfs2-config.h.in by configure.  */
/* pvfs2-config.h.in.  Generated from configure.in by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define if berkeley db error reporting was enabled */
/* #undef BERKDB_ERROR_REPORTING */

/* Define if kernel has aio support */
/* #undef HAVE_AIO */

/* Define if aiocb->__error_code exists */
/* #define HAVE_AIOCB_ERROR_CODE 1 */

/* Define if aiocb->__return_value exists */
/* #define HAVE_AIOCB_RETURN_VALUE 1 */

/* Define if VFS AIO support in kernel has a new prototype */
/* #undef HAVE_AIO_NEW_AIO_SIGNATURE */

/* Define if we are enabling VFS AIO support in kernel */
/* #undef HAVE_AIO_VFS_SUPPORT */

/* Define if read_descriptor_t has an arg member */
/* #undef HAVE_ARG_IN_READ_DESCRIPTOR_T */

/* Define to 1 if you have the <arpa/inet.h> header file. */
/* #define HAVE_ARPA_INET_H 1 */

/* Define to 1 if you have the <asm/ioctl32.h> header file. */
/* #undef HAVE_ASM_IOCTL32_H */

/* Define to 1 if you have the <attr/xattr.h> header file. */
/* #undef HAVE_ATTR_XATTR_H */

/* Define if kernel backing_dev_info struct has a name field */
/* #undef HAVE_BACKING_DEV_INFO_NAME */

/* Define if bdi_init function is present */
/* #undef HAVE_BDI_INIT */

/* Define if struct backing_dev_info in kernel has memory_backed */
/* #undef HAVE_BDI_MEMORY_BACKED */

/* Define if struct file_operations has combined aio_read and readv functions
   */
/* #undef HAVE_COMBINED_AIO_AND_VECTOR */

/* Define if there exists a compat_ioctl member in file_operations */
/* #undef HAVE_COMPAT_IOCTL_HANDLER */

/* Define if third param (message) to DB error callback function is const */
#define HAVE_CONST_THIRD_PARAMETER_TO_DB_ERROR_CALLBACK 1

/* Define if cred.h contains current_fsuid */
/* #undef HAVE_CURRENT_FSUID */

/* Define if DB error callback function takes dbenv parameter */
#define HAVE_DBENV_PARAMETER_TO_DB_ERROR_CALLBACK 1

/* Define if db library has DB_BUFFER_SMALL error */
#define HAVE_DB_BUFFER_SMALL 1

/* Define if db library has DB_DIRTY_READ flag */
#define HAVE_DB_DIRTY_READ 1

/* Define if DB has get_pagesize function */
#define HAVE_DB_GET_PAGESIZE 1

/* Define if super_operations statfs has dentry argument */
/* #undef HAVE_DENTRY_STATFS_SOP */

/* Define if kernel super_operations contains drop_inode field */
/* #undef HAVE_DROP_INODE */

/* Define if dcache.h contains d_alloc_annon */
/* #undef HAVE_D_ALLOC_ANON */

/* Define if export_operations has an encode_fh member */
/* #undef HAVE_ENCODEFH_EXPORT_OPERATIONS */

/* Define to 1 if you have the <execinfo.h> header file. */
/* #define HAVE_EXECINFO_H 1 */

/* Define to 1 if you have the `fgetxattr' function. */
/* #define HAVE_FGETXATTR 1 */

/* Define if fgetxattr takes position and option arguments */
/* #undef HAVE_FGETXATTR_EXTRA_ARGS */

/* Define if system provides fgtxattr prototype */
/* #define HAVE_FGETXATTR_PROTOTYPE 1 */

/* Define if export_operations has an fh_to_dentry member */
/* #undef HAVE_FHTODENTRY_EXPORT_OPERATIONS */

/* Define if struct inode_operations in kernel has fill_handle callback */
/* #undef HAVE_FILL_HANDLE_INODE_OPERATIONS */

/* Define if struct super_operations in kernel has find_inode_handle callback
   */
/* #undef HAVE_FIND_INODE_HANDLE_SUPER_OPERATIONS */

/* Define to 1 if you have the `fsetxattr' function. */
/* #define HAVE_FSETXATTR 1 */

/* Define if fsetxattr takes position and option arguments */
/* #undef HAVE_FSETXATTR_EXTRA_ARGS */

/* Define to 1 if you have the <fstab.h> header file. */
/* #define HAVE_FSTAB_H 1 */

/* Define if kernel has generic_file_readv */
/* #undef HAVE_GENERIC_FILE_READV */

/* Define if kernel has generic_getxattr */
/* #undef HAVE_GENERIC_GETXATTR */

/* Define if kernel has generic_permission */
/* #undef HAVE_GENERIC_PERMISSION */

/* Define if struct inode_operations in kernel has getattr_lite callback */
/* #undef HAVE_GETATTR_LITE_INODE_OPERATIONS */

/* Define if gethostbyaddr function exists */
#define HAVE_GETHOSTBYADDR 1

/* Define if gethostbyname function exists */
#define HAVE_GETHOSTBYNAME 1

/* Define to 1 if you have the `getmntent' function. */
/* #define HAVE_GETMNTENT 1 */

/* Define if pwd.h exists */
/* #define HAVE_GETPWUID 1 */

/* Define if struct super_operations in kernel has get_fs_key callback */
/* #undef HAVE_GET_FS_KEY_SUPER_OPERATIONS */

/* Define if strerror_r is GNU-specific */
/* #undef HAVE_GNU_STRERROR_R */

/* Define if libibverbs has reregister event */
/* #undef HAVE_IBV_EVENT_CLIENT_REREGISTER */

/* Define if libibverbs has ibv_get_devices */
/* #undef HAVE_IBV_GET_DEVICES */

/* Define if IB wrap_common.h exists. */
/* #undef HAVE_IB_WRAP_COMMON_H */

/* Define if kernel has iget4_locked */
/* #undef HAVE_IGET4_LOCKED */

/* Define if kernel has iget5_locked */
/* #undef HAVE_IGET5_LOCKED */

/* Define if kernel has iget_locked */
/* #undef HAVE_IGET_LOCKED */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #define HAVE_INTTYPES_H 1 */

/* Define if sceond argument to releasepage in address_space_operations is
   type int */
/* #undef HAVE_INT_ARG2_ADDRESS_SPACE_OPERATIONS_RELEASEPAGE */

/* Define if return type of invalidatepage should be int */
/* #undef HAVE_INT_RETURN_ADDRESS_SPACE_OPERATIONS_INVALIDATEPAGE */

/* Define if return value from follow_link in inode_operations is type int */
/* #undef HAVE_INT_RETURN_INODE_OPERATIONS_FOLLOW_LINK */

/* Define if return value from kmem_cache_destroy is type int */
/* #undef HAVE_INT_RETURN_KMEM_CACHE_DESTROY */

/* Define if struct inode in kernel has i_blksize member */
/* #undef HAVE_I_BLKSIZE_IN_STRUCT_INODE */

/* Define if struct inode in kernel has i_sem member */
/* #undef HAVE_I_SEM_IN_STRUCT_INODE */

/* Define if kernel has i_size_read */
/* #undef HAVE_I_SIZE_READ */

/* Define if kernel has i_size_write */
/* #undef HAVE_I_SIZE_WRITE */

/* Define if kernel has device classes */
/* #undef HAVE_KERNEL_DEVICE_CLASSES */

/* Define if kernel kmem_cache_create constructor has newer-style
   one-parameter form */
/* #undef HAVE_KMEM_CACHE_CREATE_CTOR_ONE_PARAM */

/* Define if kernel kmem_cache_create constructor has new-style two-parameter
   form */
/* #undef HAVE_KMEM_CACHE_CREATE_CTOR_TWO_PARAM */

/* Define if kernel kmem_cache_create has destructor param */
/* #undef HAVE_KMEM_CACHE_CREATE_DESTRUCTOR_PARAM */

/* Define if kmem_cache_destroy returns int */
/* #undef HAVE_KMEM_CACHE_DESTROY_INT_RETURN */

/* Define if kzalloc exists */
/* #undef HAVE_KZALLOC */

/* Define to 1 if you have the `efence' library (-lefence). */
/* #undef HAVE_LIBEFENCE */

/* Define to 1 if you have the <linux/compat.h> header file. */
/* #undef HAVE_LINUX_COMPAT_H */

/* Define to 1 if you have the <linux/exportfs.h> header file. */
/* #undef HAVE_LINUX_EXPORTFS_H */

/* Define to 1 if you have the <linux/ioctl32.h> header file. */
/* #undef HAVE_LINUX_IOCTL32_H */

/* Define to 1 if you have the <linux/mount.h> header file. */
/* #undef HAVE_LINUX_MOUNT_H */

/* Define to 1 if you have the <linux/posix_acl.h> header file. */
/* #undef HAVE_LINUX_POSIX_ACL_H */

/* Define to 1 if you have the <linux/posix_acl_xattr.h> header file. */
/* #undef HAVE_LINUX_POSIX_ACL_XATTR_H */

/* Define to 1 if you have the <linux/syscalls.h> header file. */
/* #undef HAVE_LINUX_SYSCALLS_H */

/* Define to 1 if you have the <linux/xattr_acl.h> header file. */
/* #undef HAVE_LINUX_XATTR_ACL_H */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define if kernel defines mapping_nrpages macro -- defined by RT linux */
/* #undef HAVE_MAPPING_NRPAGES_MACRO */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <mntent.h> header file. */
/* #define HAVE_MNTENT_H 1 */

/* Define to 1 if you have the <mntent.h,> header file. */
/* #undef HAVE_MNTENT_H_ */

/* Define if mount.h contains MNT_NOATIME flags */
/* #undef HAVE_MNT_NOATIME */

/* Define if mount.h contains MNT_NODIRATIME flags */
/* #undef HAVE_MNT_NODIRATIME */

/* Define to 1 if you have the <netdb.h> header file. */
/* #define HAVE_NETDB_H 1 */

/* Define if including linux/config.h gives no warnings */
/* #undef HAVE_NOWARNINGS_WHEN_INCLUDING_LINUX_CONFIG_H */

/* Define if FS_IOC flags missing from fs.h */
/* #undef HAVE_NO_FS_IOC_FLAGS */

/* Define if struct page defines a count member without leading underscore */
/* #undef HAVE_OBSOLETE_STRUCT_PAGE_COUNT_NO_UNDERSCORE */

/* Define to 1 if you have the <openssl/crypto.h> header file. */
/* #undef HAVE_OPENSSL_CRYPTO_H */

/* Define to 1 if you have the <openssl/evp.h> header file. */
/* #undef HAVE_OPENSSL_EVP_H */

/* Define if sysctl proc handlers have 6th argument */
/* #undef HAVE_PROC_HANDLER_FILE_ARG */

/* Define if sysctl proc handlers have ppos argument */
/* #undef HAVE_PROC_HANDLER_PPOS_ARG */

/* Define if have PtlACEntry with jid argument. */
/* #undef HAVE_PTLACENTRY_JID */

/* Define to 1 if you have the `PtlErrorStr' function. */
/* #undef HAVE_PTLERRORSTR */

/* Define to 1 if you have the `PtlEventKindStr' function. */
/* #undef HAVE_PTLEVENTKINDSTR */

/* Define if kernel super_operations contains put_inode field */
/* #undef HAVE_PUT_INODE */

/* Define if struct file_operations in kernel has readdirplus_lite callback */
/* #undef HAVE_READDIRPLUSLITE_FILE_OPERATIONS */

/* Define if struct file_operations in kernel has readdirplus callback */
/* #undef HAVE_READDIRPLUS_FILE_OPERATIONS */

/* Define if struct file_operations in kernel has readv callback */
/* #undef HAVE_READV_FILE_OPERATIONS */

/* Define if struct file_operations in kernel has readx callback */
/* #undef HAVE_READX_FILE_OPERATIONS */

/* Define if kernel super_operations contains read_inode field */
/* #undef HAVE_READ_INODE */

/* Define if kernel has register_ioctl32_conversion */
/* #undef HAVE_REGISTER_IOCTL32_CONVERSION */

/* Define if kernel address_space struct has a spin_lock for private data
   instead of rw_lock -- used by RT linux */
/* #undef HAVE_RT_PRIV_LOCK_ADDR_SPACE_STRUCT */

/* Define if kernel address_space struct has a rw_lock_t member named
   tree_lock */
/* #undef HAVE_RW_LOCK_TREE_ADDR_SPACE_STRUCT */

/* Define if struct super_block has s_dirty list */
/* #undef HAVE_SB_DIRTY_LIST */

/* Define to 1 if you have the <SDL/SDL_ttf.h> header file. */
/* #undef HAVE_SDL_SDL_TTF_H */

/* Define to 1 if you have the <SDL_ttf.h> header file. */
/* #undef HAVE_SDL_TTF_H */

/* Define if struct file_operations in kernel has sendfile callback */
/* #undef HAVE_SENDFILE_VFS_SUPPORT */

/* Define if kernel setxattr has const void* argument */
/* #undef HAVE_SETXATTR_CONST_ARG */

/* Define if SLAB_KERNEL is defined in kernel */
/* #undef HAVE_SLAB_KERNEL */

/* Define if kernel address_space struct has a spin_lock member named
   page_lock instead of rw_lock */
/* #undef HAVE_SPIN_LOCK_PAGE_ADDR_SPACE_STRUCT */

/* Define if kernel address_space struct has a spin_lock_t member named
   tree_lock */
/* #undef HAVE_SPIN_LOCK_TREE_ADDR_SPACE_STRUCT */

/* Define if struct super_operations in kernel has statfs_lite callback */
/* #undef HAVE_STATFS_LITE_SUPER_OPERATIONS */

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtoull' function. */
#define HAVE_STRTOULL 1

/* Define if struct kmem_cache is defined in kernel */
/* #undef HAVE_STRUCT_KMEM_CACHE */

/* Define if struct xtvec is defined in the kernel */
/* #undef HAVE_STRUCT_XTVEC */

/* Define if sysinfo.h is present */
/* #define HAVE_SYSINFO 1 */

/* Define to 1 if you have the <sys/epoll.h> header file. */
/* #define HAVE_SYS_EPOLL_H 1 */

/* Define if sys/mount.h exists */
/* #define HAVE_SYS_MOUNT_H 1 */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #define HAVE_SYS_SOCKET_H 1 */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if sys/vfs.h exists */
/* #define HAVE_SYS_VFS_H 1 */

/* Define to 1 if you have the <sys/xattr.h> header file. */
/* #define HAVE_SYS_XATTR_H 1 */

/* Define if TAU library is used */
/* #undef HAVE_TAU */

/* Define if register_sysctl_table takes two arguments */
/* #undef HAVE_TWO_ARG_REGISTER_SYSCTL_TABLE */

/* Define if kernel's inode_operations has two parameters permission function
   */
/* #undef HAVE_TWO_PARAM_PERMISSION */

/* Define if DB open function takes a txnid parameter */
#define HAVE_TXNID_PARAMETER_TO_DB_OPEN 1

/* Define if DB stat function takes txnid parameter */
#define HAVE_TXNID_PARAMETER_TO_DB_STAT 1

/* Define to 1 if you have the <unistd.h> header file. */
/* #define HAVE_UNISTD_H 1 */

/* Define if DB stat function takes malloc function ptr */
/* #undef HAVE_UNKNOWN_PARAMETER_TO_DB_STAT */

/* Define if include file valgrind.h exists */
/* #undef HAVE_VALGRIND_H */

/* Define if file_system_type get_sb has vfsmount argument */
/* #undef HAVE_VFSMOUNT_GETSB */

/* Define if struct file_operations in kernel has writev callback */
/* #undef HAVE_WRITEV_FILE_OPERATIONS */

/* Define if struct file_operations in kernel has writex callback */
/* #undef HAVE_WRITEX_FILE_OPERATIONS */

/* Define if kernel has xattr support */
/* #undef HAVE_XATTR */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* major version number */
#define PVFS2_VERSION_MAJOR 2

/* minor version number */
#define PVFS2_VERSION_MINOR 8

/* sub version number */
#define PVFS2_VERSION_SUB 4

/* The size of `long int', as computed by sizeof. */
#define SIZEOF_LONG_INT 4  /* 4 on Windows */

/* The size of `void *', as computed by sizeof. */
#ifdef _WIN64
#define SIZEOF_VOID_P 8
#else
#define SIZEOF_VOID_P 4
#endif

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if on darwin */
/* #undef TARGET_OS_DARWIN */

/* Define if on linux */
/* #define TARGET_OS_LINUX 1 */


/* Define to build for linux kernel module userspace helper. */
/* #undef WITH_LINUX_KMOD */

/* Define if mtrace memory leak detection was enabled */
/* #undef WITH_MTRACE */

/* Define if openssl exists */
/* #undef WITH_OPENSSL */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif
