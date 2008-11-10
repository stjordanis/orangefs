/*
 * (C) 2001 Clemson University and The University of Chicago
 *
 * See COPYING in top-level directory.
 */

/** \addtogroup bmiint
 *
 * @{
 */

/** \file
 *  Types and other defines used throughout BMI.  Many of the BMI
 *  types are defined in terms of PVFS2 types rather than using the PVFS2
 *  types themselves.  This is because we wanted to separate the BMI
 *  package, in case it was useful in other projects.  It may be that at
 *  some later date we will make a pass through BMI and eliminate many of
 *  these "extra" types.
 */

#ifndef __BMI_TYPES_H
#define __BMI_TYPES_H

#include <stdlib.h>
#include "pvfs2-debug.h"
#include "pvfs2-types.h"

typedef PVFS_size bmi_size_t;           /**< Data region size */
typedef PVFS_msg_tag_t bmi_msg_tag_t;   /**< User-specified message tag */
typedef PVFS_context_id bmi_context_id; /**< Context identifier */
typedef PVFS_id_gen_t bmi_op_id_t;      /**< Reference to ongoing network op */

/* TODO: not using a real type for this yet; need to specify what
 * error codes look like */
typedef int32_t bmi_error_code_t; /**< error code information */

#define BMI_MAX_ADDR_LEN PVFS_MAX_SERVER_ADDR_LEN

/** BMI method initialization flags */
enum
{
    BMI_INIT_SERVER = 1, /**< set up to listen for unexpected messages */
    BMI_TCP_BIND_SPECIFIC = 2, /**< bind to a specific IP address if INIT_SERVER */
    BMI_AUTO_REF_COUNT = 4 /**< automatically increment ref count on unexpected messages */
};

enum bmi_op_type
{
    BMI_SEND = 1,
    BMI_RECV = 2
};

/** BMI memory buffer flags */
enum bmi_buffer_type
{
    BMI_PRE_ALLOC = 1,
    BMI_EXT_ALLOC = 2
};

/** BMI get_info and set_info options */
enum
{
    BMI_DROP_ADDR = 1,         /**< ask a module to discard an address */
    BMI_CHECK_INIT = 2,        /**< see if a module is initialized */
    BMI_CHECK_MAXSIZE = 3,     /**< check the max msg size a module allows */
    BMI_GET_METH_ADDR = 4,     /**< kludge to return void* pointer to
                                *   underlying module address */
    BMI_INC_ADDR_REF = 5,      /**< increment address reference count */
    BMI_DEC_ADDR_REF = 6,      /**< decrement address reference count */
    BMI_DROP_ADDR_QUERY = 7,   /**< ask a module if it thinks an address
                                *   should be discarded */
    BMI_FORCEFUL_CANCEL_MODE = 8, /**< enables a more forceful cancel mode */
#ifdef USE_TRUSTED
    BMI_TRUSTED_CONNECTION = 9, /**< allows setting the TrustedPorts and Network options */
#endif
    BMI_GET_UNEXP_SIZE = 10,     /**< get the maximum unexpected payload */
    BMI_TCP_BUFFER_SEND_SIZE = 11,
    BMI_TCP_BUFFER_RECEIVE_SIZE = 12,
    BMI_TCP_CLOSE_SOCKET = 13,
    BMI_OPTIMISTIC_BUFFER_REG = 14,
};

/** used to describe a memory region in passing down a registration
 * hint from IO routines. */
struct bmi_optimistic_buffer_info {
    const void *buffer;
    PVFS_size len;
    enum PVFS_io_type rw;
};

/* mappings from PVFS errors to BMI errors */
#define BMI_EPERM           (PVFS_EPERM | PVFS_ERROR_BMI)
#define BMI_ENOENT          (PVFS_ENOENT | PVFS_ERROR_BMI)
#define BMI_EINTR           (PVFS_EINTR | PVFS_ERROR_BMI)
#define BMI_EIO             (PVFS_EIO | PVFS_ERROR_BMI)
#define BMI_ENXIO           (PVFS_ENXIO | PVFS_ERROR_BMI)
#define BMI_EBADF           (PVFS_EBADF | PVFS_ERROR_BMI)
#define BMI_EAGAIN          (PVFS_EAGAIN | PVFS_ERROR_BMI)
#define BMI_ENOMEM          (PVFS_ENOMEM | PVFS_ERROR_BMI)
#define BMI_EFAULT          (PVFS_EFAULT | PVFS_ERROR_BMI)
#define BMI_EBUSY           (PVFS_EBUSY | PVFS_ERROR_BMI)
#define BMI_EEXIST          (PVFS_EEXIST | PVFS_ERROR_BMI)
#define BMI_ENODEV          (PVFS_ENODEV | PVFS_ERROR_BMI)
#define BMI_ENOTDIR         (PVFS_ENOTDIR | PVFS_ERROR_BMI)
#define BMI_EISDIR          (PVFS_EISDIR | PVFS_ERROR_BMI)
#define BMI_EINVAL          (PVFS_EINVAL | PVFS_ERROR_BMI)
#define BMI_EMFILE          (PVFS_EMFILE | PVFS_ERROR_BMI)
#define BMI_EFBIG           (PVFS_EFBIG | PVFS_ERROR_BMI)
#define BMI_ENOSPC          (PVFS_ENOSPC | PVFS_ERROR_BMI)
#define BMI_EROFS           (PVFS_EROFS | PVFS_ERROR_BMI)
#define BMI_EMLINK          (PVFS_EMLINK | PVFS_ERROR_BMI)
#define BMI_EPIPE           (PVFS_EPIPE | PVFS_ERROR_BMI)
#define BMI_EDEADLK         (PVFS_EDEADLK | PVFS_ERROR_BMI)
#define BMI_ENAMETOOLONG    (PVFS_ENAMETOOLONG | PVFS_ERROR_BMI)
#define BMI_ENOLCK          (PVFS_ENOLCK | PVFS_ERROR_BMI)
#define BMI_ENOSYS          (PVFS_ENOSYS | PVFS_ERROR_BMI)
#define BMI_ENOTEMPTY       (PVFS_ENOTEMPTY | PVFS_ERROR_BMI)
#define BMI_ELOOP           (PVFS_ELOOP | PVFS_ERROR_BMI)
#define BMI_EWOULDBLOCK     (PVFS_EWOULDBLOCK | PVFS_ERROR_BMI)
#define BMI_ENOMSG          (PVFS_ENOMSG | PVFS_ERROR_BMI)
#define BMI_EUNATCH         (PVFS_EUNATCH | PVFS_ERROR_BMI)
#define BMI_EBADR           (PVFS_EBADR | PVFS_ERROR_BMI)
#define BMI_EDEADLOCK       (PVFS_EDEADLOCK | PVFS_ERROR_BMI)
#define BMI_ENODATA         (PVFS_ENODATA | PVFS_ERROR_BMI)
#define BMI_ETIME           (PVFS_ETIME | PVFS_ERROR_BMI)
#define BMI_ENONET          (PVFS_ENONET | PVFS_ERROR_BMI)
#define BMI_EREMOTE         (PVFS_EREMOTE | PVFS_ERROR_BMI)
#define BMI_ECOMM           (PVFS_ECOMM | PVFS_ERROR_BMI)
#define BMI_EPROTO          (PVFS_EPROTO | PVFS_ERROR_BMI)
#define BMI_EBADMSG         (PVFS_EBADMSG | PVFS_ERROR_BMI)
#define BMI_EOVERFLOW       (PVFS_EOVERFLOW | PVFS_ERROR_BMI)
#define BMI_ERESTART        (PVFS_ERESTART | PVFS_ERROR_BMI)
#define BMI_EMSGSIZE        (PVFS_EMSGSIZE | PVFS_ERROR_BMI)
#define BMI_EPROTOTYPE      (PVFS_EPROTOTYPE | PVFS_ERROR_BMI)
#define BMI_ENOPROTOOPT     (PVFS_ENOPROTOOPT | PVFS_ERROR_BMI)
#define BMI_EPROTONOSUPPORT (PVFS_EPROTONOSUPPORT | PVFS_ERROR_BMI)
#define BMI_EOPNOTSUPP      (PVFS_EOPNOTSUPP | PVFS_ERROR_BMI)
#define BMI_EADDRINUSE      (PVFS_EADDRINUSE | PVFS_ERROR_BMI)
#define BMI_EADDRNOTAVAIL   (PVFS_EADDRNOTAVAIL | PVFS_ERROR_BMI)
#define BMI_ENETDOWN        (PVFS_ENETDOWN | PVFS_ERROR_BMI)
#define BMI_ENETUNREACH     (PVFS_ENETUNREACH | PVFS_ERROR_BMI)
#define BMI_ENETRESET       (PVFS_ENETRESET | PVFS_ERROR_BMI)
#define BMI_ENOBUFS         (PVFS_ENOBUFS | PVFS_ERROR_BMI)
#define BMI_ECONNRESET      (PVFS_ECONNRESET | PVFS_ERROR_BMI)
#define BMI_ETIMEDOUT       (PVFS_ETIMEDOUT | PVFS_ERROR_BMI)
#define BMI_ECONNREFUSED    (PVFS_ECONNREFUSED | PVFS_ERROR_BMI)
#define BMI_EHOSTDOWN       (PVFS_EHOSTDOWN | PVFS_ERROR_BMI)
#define BMI_EHOSTUNREACH    (PVFS_EHOSTUNREACH | PVFS_ERROR_BMI)
#define BMI_EALREADY        (PVFS_EALREADY | PVFS_ERROR_BMI)
#define BMI_EACCES          (PVFS_EACCES | PVFS_ERROR_BMI)

#define BMI_EREMOTEIO       (PVFS_EREMOTEIO | PVFS_ERROR_BMI)
#define BMI_ENOMEDIUM       (PVFS_ENOMEDIUM | PVFS_ERROR_BMI)
#define BMI_EMEDIUMTYPE     (PVFS_EMEDIUMTYPE | PVFS_ERROR_BMI)

#define BMI_ECANCEL	    (PVFS_ECANCEL | PVFS_ERROR_BMI)
#define BMI_EDEVINIT	    (PVFS_EDEVINIT | PVFS_ERROR_BMI)
#define BMI_EDETAIL	    (PVFS_EDETAIL | PVFS_ERROR_BMI)
#define BMI_EHOSTNTFD	    (PVFS_EHOSTNTFD | PVFS_ERROR_BMI)
#define BMI_EADDRNTFD	    (PVFS_EADDRNTFD | PVFS_ERROR_BMI)
#define BMI_ENORECVR	    (PVFS_ENORECVR | PVFS_ERROR_BMI)
#define BMI_ETRYAGAIN	    (PVFS_ETRYAGAIN | PVFS_ERROR_BMI)

/** default bmi error translation function */
int bmi_errno_to_pvfs(int error);

#endif /* __BMI_TYPES_H */

/* @} */

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
