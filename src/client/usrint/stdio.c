/* 
 * (C) 2011 Clemson University and The University of Chicago 
 *
 * See COPYING in top-level directory.
 */

/** \file
 *  \ingroup usrint
 *
 *  PVFS2 user interface routines - implementation of stdio for pvfs
 */
/* this prevents headers from using inlines for 64 bit calls */
#define USRINT_SOURCE 1
#include "usrint.h"
#include "openfile-util.h"
#include "stdio-ops.h"

/* fdopendir not present until glibc2.5 */
#if __GLIBC_PREREQ (2,5)
#else
extern DIR *fdopendir (int __fd);
#endif


#ifdef gossip_debug
#undef gossip_debug
#endif

#ifdef GOSSIP_USRINT_DEBUG
#undef GOSSIP_USRINT_DEBUG
#endif

#ifdef USRINT_DEBUG
#define GOSSIP_USRINT_DEBUG stderr
#define gossip_debug fprintf
#else
#define gossip_debug(__m, __f, ...)
#endif /* USRINT_DEBUG */

#define PVFS_STDIO_DEBUG 0

static void init_stdio(void);
static struct stdio_ops_s stdio_ops;
static FILE open_files = {._chain = NULL};

#define _P_IO_MAGIC     0xF0BD0000
#define SETMAGIC(s,m)   do{(s)->_flags = (m) & _IO_MAGIC_MASK;}while(0)
#define ISMAGICSET(s,m) (((s)->_flags & _IO_MAGIC_MASK) == (m))
#define SETFLAG(s,f)    do{(s)->_flags |= ((f) & ~_IO_MAGIC_MASK);}while(0)
#define CLEARFLAG(s,f)  do{(s)->_flags &= ~((f) & ~_IO_MAGIC_MASK);}while(0)
#define ISFLAGSET(s,f)  (((s)->_flags & (f)) == (f))

/* STDIO implementation - this gives users something to link to
 * that will call back to the PVFS lib - also lets us optimize
 * in a few spots like buffer sizes and stuff
 */

/** struct representing a directory stream for buffered dir io
 *
 * this struct type is undefined in /usr/include as it is opaque
 * it is defined in this file only.  This design is based loosely
 * on the buffered IO scheme used in Linux for files.
 */
struct __dirstream {
    int _flags;       /**< general flags field */
    int fileno;       /**< file dscriptor of open dir */
    struct dirent de; /**< pointer to dirent read by readdir */
    char *buf_base;   /**< pointer to beginning of buffer */
    char *buf_end;    /**< pointer to end of buffer */
    char *buf_act;    /**< pointer to end of active portion of buffer */
    char *buf_ptr;    /**< pointer to current position in buffer */
};

#define DIRSTREAM_MAGIC 0xFD100000
#define PVFS_RLDC PVFS_REQ_LIMIT_DIRENT_COUNT
#define MAXDIRENT (PVFS_RLDC < 512 ? PVFS_RLDC : 512)
#define MAXBUFSIZE (MAXDIRENT * sizeof(struct dirent64))
#define DIRBUFSIZE ((MAXBUFSIZE / 1024) * 1024)
#define ASIZE 256
#define MAXTRIES 16 /* arbitrary - how many tries to get a unique file name */

/** These functions lock and unlock the stream structure
 *
 *  These are only called within our library, so we assume that the
 *  stream is good, that it is our stream (and not glibc's) and we
 *  check for the flag to see if the lock is being used.
 */

static inline void lock_init_stream(FILE *stream)
{
#ifdef _IO_MTSAFE_IO
    if (ISFLAGSET(stream, _IO_USER_LOCK))
    {
        _IO_lock_init(stream->_lock);
    }
#endif
}

static inline void lock_stream(FILE *stream)
{
#ifdef _IO_MTSAFE_IO
    if (ISFLAGSET(stream, _IO_USER_LOCK))
    {
        _IO_lock_lock(stream->_lock);
    }
#endif
}

static inline int trylock_stream(FILE *stream)
{
#ifdef _IO_MTSAFE_IO
    if (ISFLAGSET(stream, _IO_USER_LOCK))
    {
        return _IO_lock_try(stream->_lock);
    }
#else
    return 0;
#endif
}

static inline void unlock_stream(FILE *stream)
{
#ifdef _IO_MTSAFE_IO
    if (ISFLAGSET(stream, _IO_USER_LOCK))
    {
        _IO_lock_unlock(stream->_lock);
    }
#endif
}

static inline void lock_fini_stream(FILE *stream)
{
#ifdef _IO_MTSAFE_IO
    if (ISFLAGSET(stream, _IO_USER_LOCK))
    {
        _IO_lock_fini(stream->_lock);
    }
#endif
}

/** POSIX interface for user level locking of streams *.
 *
 */
void flockfile(FILE *stream)
{
    lock_stream(stream);
}

int ftrylockfile(FILE *stream)
{
    return trylock_stream(stream);
}

void funlockfile(FILE *stream)
{
    unlock_stream(stream);
}

/** This function converts from stream style mode to syscall
 *  style flags
 *
 */
static int mode2flags(const char *mode)
{
    int i = 0;
    int flags = 0;
    int append = false, read = false, write = false, update = false;
    int exclusive = false;

    /* look for fopen modes */ 
    for(i = 0; mode[i]; i++)
    { 
        switch(mode[i]) { 
            case 'a':
                append = true; 
                if (read || write)
                {
                    errno = EINVAL;
                    return -1;
                }
                break; 
            case 'r':
                read = true; 
                if (append || write)
                {
                    errno = EINVAL;
                    return -1;
                }
                break; 
            case 'w':
                write = true; 
                if (read || append)
                {
                    errno = EINVAL;
                    return -1;
                }
                break; 
            case '+':
                update = true; 
                if (!(read || write || append))
                {
                    errno = EINVAL;
                    return -1;
                }
                break;
            case 'b': /* this is ignored in POSIX */
            case 'c': /* used in glibc ignored here */
            case 'e': /* used in glibc ignored here */
            case 'm': /* used in glibc ignored here */
                break;
            case 'x': /* glibc extension */
                exclusive = true;
                if (!(read || write || append))
                {
                    errno = EINVAL;
                    return -1;
                }
                break;
            default:
                errno = EINVAL;
                return -1;
                break; 
        }
    }
    /* this catches an empty mode */
    if (!(read || write || append))
    {
        errno = EINVAL;
        return -1;
    }
    if (read && update)
    { 
        flags = O_RDWR; 
    }
    else if(read)
    { 
        flags = O_RDONLY; 
    }
    else if(write && update)
    { 
        flags = O_RDWR | O_CREAT | O_TRUNC; 
    } 
    else if(write)
    { 
        flags = O_WRONLY | O_CREAT | O_TRUNC; 
    }
    else if(append && update)
    { 
        flags = O_RDWR | O_APPEND | O_CREAT; 
    } 
    else if (append)
    { 
        flags = O_WRONLY | O_APPEND | O_CREAT; 
    }
    if (exclusive) /* check this regardless of the above */
    {
        flags |= O_EXCL;
    }
    return flags;
}
 
/**
 * fopen opens a file, then adds a stream to it
 */ 
FILE *fopen(const char *path, const char *mode)
{ 
    int fd = 0;
    int flags = 0;
    FILE *newfile = NULL;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fopen %s %s\n", path, mode);
    flags = mode2flags(mode);
    if (flags == -1)
    {
        return NULL;
    }

    fd = open(path, flags, 0666);
    if (fd == -1)
    {
        return NULL;
    }

    newfile = fdopen(fd, mode);

    return newfile;
}

/**
 * fopen64 - not clear why there is an fopen64 but there is
 */
FILE *fopen64(const char *path, const char *modes)
{
    return fopen(path, modes);
}

/** this function sets up a new stream's buffer area
 *
 */
static int init_stream (FILE *stream, int flags, int bufsize)
{
    /* set up stream here */
    SETMAGIC(stream, _P_IO_MAGIC);
    if (!(flags & O_WRONLY))
        SETFLAG(stream, _IO_NO_READS);
    if (!(flags & O_RDONLY))
        SETFLAG(stream, _IO_NO_WRITES);
    /* set up default buffering here */
    stream->_IO_buf_base   = (char *)malloc(bufsize);
    if (!stream->_IO_buf_base)
    {
        return -1;
    }
    stream->_IO_buf_end    = stream->_IO_buf_base + bufsize;
    stream->_IO_read_base  = stream->_IO_buf_base;
    stream->_IO_read_ptr   = stream->_IO_buf_base;
    stream->_IO_read_end   = stream->_IO_buf_base;
    stream->_IO_write_base = stream->_IO_buf_base;
    stream->_IO_write_ptr  = stream->_IO_buf_base;
    stream->_IO_write_end  = stream->_IO_buf_end;
    lock_stream(&open_files);
    stream->_chain = open_files._chain;
    open_files._chain = stream;
    unlock_stream(&open_files);
    return 0;
}

/**
 * fdopen adds a stream to an existing open file
 */
FILE *fdopen(int fd, const char *mode)
{
    int rc = 0;
    FILE *newfile = NULL;
    int flags;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fdopen %d %s\n", fd, mode);
    /* need to check for valid mode here */
    /* it must be compatible with the existing mode */
    flags = mode2flags(mode);

    newfile = (FILE *)malloc(sizeof(FILE));
    if (!newfile)
    {
        errno = ENOMEM;
        return NULL; 
    }
    memset(newfile, 0, sizeof(FILE));

    /* initize lock for this stream */
    SETFLAG(newfile, _IO_USER_LOCK);
    lock_init_stream(newfile);

    newfile->_fileno = fd;
    rc = init_stream(newfile, flags, PVFS_BUFSIZE);
    if(rc)
    {
        free(newfile);
        return NULL;
    }
    return newfile;
}

/**
 * freopen closes the file and opens another one for the stream
 */
FILE *freopen(const char *path, const char *mode, FILE *stream)
{
    int fd = 0;
    int flags = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "freopen %s %s %p\n", path, mode, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.freopen(path, mode, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.freopen(path, mode, stream);
        }
        errno = EINVAL;
        return NULL;
    }
    lock_stream(stream);
    /* see if stream is in use - if so close the file */
    if (stream->_fileno > -1)
    {
        int rc;
        rc = close(stream->_fileno);
        if (rc == -1)
        {
            unlock_stream(stream);
            return NULL;
        }
    }

    /* translate mode to flags */
    flags = mode2flags(mode);

    /* open the file */
    fd = open(path, flags, 0666);
    if (fd == -1)
    {
        unlock_stream(stream);
        return NULL;
    }
    stream->_fileno = fd;

    /* reset buffering here */
    if (stream->_IO_buf_base)
        free (stream->_IO_buf_base);
    init_stream(stream, flags, PVFS_BUFSIZE);

    unlock_stream(stream);
    return stream;
}

/**
 * freopen64 - again this appears useless but nevertheless ...
 */
FILE *freopen64 (const char *path, const char *modes, FILE *stream)
{
    return freopen(path, modes, stream);
}

/**
 * These functions do not need PVFS versions and thus
 * are not implemented here
 */
#if 0
FILE *fopencookie(void *cookie, const char *modes,
                    _IO_cookie_io_function_t funcs);
FILE *fmemopen(void *buf, size_t size, const char *mode);
FILE *open_memstream(char **ptr, size_t *sizeloc);
#endif

/** Implements buffered write using Linux pointer model
 * 
 *  Two sets of pointers, one for reading one for writing
 *  flag determins which mode we are in.  start always 
 *  points to beginning of buffer, end points to end
 *  In read, end points to end of actual data read and
 *  coincides with the file pointer.  In write the start
 *  coincides with file pointer.  In either case ptr is
 *  where user stream pointer is.
 *
 *  The FILE struct is struct _IO_FILE defined in /usr/include/libio.h
 */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int rc = 0;

    /* causing loops */
    /* gossip_debug(GOSSIP_USRINT_DEBUG, "fwrite %p %d %d %p\n",
                    ptr, size, nmemb, stream); */
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fwrite(ptr, size, nmemb, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fwrite(ptr, size, nmemb, stream);
        }
        errno = EINVAL;
        return 0;
    }
    lock_stream(stream);
    rc = fwrite_unlocked(ptr, size, nmemb, stream);
    unlock_stream(stream);
    return rc;
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    off64_t rsz, rsz_buf, rsz_extra;
    int rc;

    /* causing loops */
    /* gossip_debug(GOSSIP_USRINT_DEBUG, "fwrite_unlocked %p %d %d %p\n",
                    ptr, size, nmemb, stream); */
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fwrite_unlocked(ptr, size, nmemb, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fwrite_unlocked(ptr, size, nmemb, stream);
        }
        errno = EINVAL;
        return 0;
    }
    if (!ptr || size <= 0 || nmemb <= 0)
    {
        errno = EINVAL;
        return 0;
    }

    /* Check to see if switching from read to write */
    if (!ISFLAGSET(stream, _IO_CURRENTLY_PUTTING))
    {
        /* reset read pointer */
        stream->_IO_read_ptr = stream->_IO_read_end;
        /* set flag */
        SETFLAG(stream, _IO_CURRENTLY_PUTTING);
        /* indicate read buffer empty */
        stream->_IO_read_end = stream->_IO_read_base;
        stream->_IO_read_ptr = stream->_IO_read_end;
        /* indicate write buffer empty */
        stream->_IO_write_end = stream->_IO_buf_end;
        stream->_IO_write_ptr = stream->_IO_write_base;
    }

    rsz = size * nmemb;
    rsz_buf = PVFS_util_min(rsz, stream->_IO_write_end - stream->_IO_write_ptr);
    rsz_extra = rsz - rsz_buf;

    if (rsz_buf) /* is only zero if buffer is full */
    {
        memcpy(stream->_IO_write_ptr, ptr, rsz_buf);
        stream->_IO_write_ptr += rsz_buf;
    }

    /* if there is more to write */
    if (rsz_extra)
    {
        /* buffer is full - write the current buffer */
#if PVFS_STDIO_DEBUG
        fprintf(stderr,"fwrite writing %d bytes to offset %d\n",
                    (int)(stream->_IO_write_ptr - stream->_IO_write_base),
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
        rc = write(stream->_fileno, stream->_IO_write_base,
                        stream->_IO_write_ptr - stream->_IO_write_base);
        if (rc == -1)
        {
            SETFLAG(stream, _IO_ERR_SEEN);
            return 0;
        }
        /* TODO: check for a short write */
        /* reset buffer */
        stream->_IO_write_ptr = stream->_IO_write_base;
        /* if there more data left in request than fits in a buffer */
        if(rsz_extra > stream->_IO_buf_end - stream->_IO_buf_base)
        {
#if PVFS_STDIO_DEBUG
            fprintf(stderr,"fwrite writing %d bytes to offset %d\n",
                    (int)rsz_extra,
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
            /* write data directly */
            rc = write(stream->_fileno, ptr + rsz_buf, rsz_extra);
            if (rc == -1)
            {
                SETFLAG(stream, _IO_ERR_SEEN);
                return 0;
            }
            /* TODO: check for a short write */
        }
        else
        {
            memcpy(stream->_IO_write_ptr, ptr + rsz_buf, rsz_extra);
            stream->_IO_write_ptr += rsz_extra;
        }
    }
    
    return rsz / size; /* num items written */
}

/*
 * fread implements the same buffer scheme as in fwrite
 */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fread %p %d %d %p\n",
                    ptr, size, nmemb, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fread(ptr, size, nmemb, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fread(ptr, size, nmemb, stream);
        }
        errno = EINVAL;
        return 0;
    }
    lock_stream(stream);
    rc = fread_unlocked(ptr, size, nmemb, stream);
    unlock_stream(stream);
    return rc;
}

size_t fread_unlocked(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int rsz, rsz_buf, rsz_extra;
    int bytes_read;
    int rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fread_unlocked %p %d %d %p\n",
                    ptr, size, nmemb, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fread_unlocked(ptr, size, nmemb, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fread_unlocked(ptr, size, nmemb, stream);
        }
        errno = EINVAL;
        return 0;
    }
    if (!ptr || size < 0 || nmemb < 0)
    {
        errno = EINVAL;
        return 0;
    }

    /* Check to see if switching from write to read */
    if (ISFLAGSET(stream, _IO_CURRENTLY_PUTTING))
    {
        /* write buffer back */
#if PVFS_STDIO_DEBUG
        fprintf(stderr,"fread writing %d bytes to offset %d\n",
                    (int)(stream->_IO_write_ptr - stream->_IO_write_base),
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
        rc = write(stream->_fileno, stream->_IO_write_base,
                stream->_IO_write_ptr - stream->_IO_write_base); 
        if (rc == -1)
        {
            SETFLAG(stream, _IO_ERR_SEEN);
            return 0;
        }
        else if (rc < stream->_IO_write_ptr - stream->_IO_write_base)
        {
            /* short write but no error ??? */
            SETFLAG(stream, _IO_ERR_SEEN);
        }
        /* reset write pointer */
        stream->_IO_write_ptr = stream->_IO_write_base;
        /* clear flag */
        CLEARFLAG(stream, _IO_CURRENTLY_PUTTING);
        /* indicate read buffer empty */
        stream->_IO_read_end = stream->_IO_read_base;
        stream->_IO_read_ptr = stream->_IO_read_end;
    }

    /* see if anything is in read buffer */
    if (stream->_IO_read_end == stream->_IO_read_base ||
        stream->_IO_read_ptr == stream->_IO_read_end)
    {
        /* buffer empty so read new buffer */
        bytes_read = read(stream->_fileno, stream->_IO_read_base,
                stream->_IO_buf_end - stream->_IO_buf_base);
        if (bytes_read == -1)
        {
            SETFLAG(stream, _IO_ERR_SEEN);
            return 0;
        }
        else if (bytes_read == 0)
        {
            SETFLAG(stream, _IO_EOF_SEEN);
        }
        /* indicate end of read area */
        stream->_IO_read_end = stream->_IO_read_base + bytes_read;
        /* reset read pointer */
        stream->_IO_read_ptr = stream->_IO_read_base;
    }

    /* 
     * we assume there is a block in the buffer now
     * and that the current file pointer corresponds
     * to the end of the read buffer.  The user has
     * only seen up to the read pointer.
     */
    rsz = size * nmemb;  /* total bytes requested */
    rsz_buf = PVFS_util_min(rsz, stream->_IO_read_end - stream->_IO_read_ptr);
    rsz_extra = rsz - rsz_buf;  /* bytes beyond the buffer */
    
    /* copy rz_buf bytes from buffer */
    if (rsz_buf) /* zero if at EOF */
    {
        memcpy(ptr, stream->_IO_read_ptr, rsz_buf);
        stream->_IO_read_ptr += rsz_buf;
    }

    /* if more bytes requested */
    if (rsz_extra)
    {
        /* if current buffer not at EOF */
        if (stream->_IO_read_end == stream->_IO_buf_end)
        {
            /* if more data requested than fits in buffer */
            if (rsz_extra > (stream->_IO_buf_end - stream->_IO_buf_base))
            {
                /* read directly from file for remainder of request */
                bytes_read = read(stream->_fileno, ptr+rsz_buf, rsz_extra);
                if (bytes_read == -1)
                {
                    SETFLAG(stream, _IO_ERR_SEEN);
                    return 0;
                }
                else if (bytes_read == 0)
                {
                    SETFLAG(stream, _IO_EOF_SEEN);
                }
                if (bytes_read == rsz_extra)
                {
                    /* then read next buffer */
                    bytes_read = read(stream->_fileno, stream->_IO_buf_base,
                            stream->_IO_buf_end - stream->_IO_buf_base);
                    if (bytes_read == -1)
                    {
                        SETFLAG(stream, _IO_ERR_SEEN);
                        return 0;
                    }
                    else if (bytes_read == 0)
                    {
                        SETFLAG(stream, _IO_EOF_SEEN);
                    }
                    stream->_IO_read_end = stream->_IO_read_base + bytes_read;
                    stream->_IO_read_ptr = stream->_IO_read_base;
                    return rsz / size; /* num items read */
                }
                /* MIGHT have read to EOF - check for pipe, tty */
                SETFLAG(stream, _IO_EOF_SEEN);
                return (rsz_buf + bytes_read) / size; /* num items read */
            }
            /* rest of request fits in a buffer - read next buffer */
            bytes_read = read(stream->_fileno, stream->_IO_buf_base,
                    stream->_IO_buf_end - stream->_IO_buf_base);
            if (bytes_read == -1)
            {
                SETFLAG(stream, _IO_ERR_SEEN);
                return 0;
            }
            else if (bytes_read == 0)
            {
                SETFLAG(stream, _IO_EOF_SEEN);
            }
            stream->_IO_read_end = stream->_IO_read_base + bytes_read;
            stream->_IO_read_ptr = stream->_IO_read_base;
            /* transfer remainder */
            rsz_extra = PVFS_util_min(rsz_extra,
                    stream->_IO_read_end - stream->_IO_read_ptr);
            if (rsz_extra) /* zero if at EOF */
            {
                memcpy(ptr, stream->_IO_read_ptr, rsz_extra);
                stream->_IO_read_ptr += rsz_extra;
            }
            /* MIGHT have read to EOF - check for pipe, tty */
            if (rsz_buf + rsz_extra < rsz)
            {
                SETFLAG(stream, _IO_EOF_SEEN);
            }
            return (rsz_buf + rsz_extra) / size; /* num items read */
        }
        else
        {
            /* at EOF so return bytes read */
            SETFLAG(stream, _IO_EOF_SEEN);
            return rsz_buf / size; /* num items read */
        }
    }
    /* request totally within current buffer */
    return rsz / size; /* num items read */
}

/**
 * fcloseall closes all open streams
 */
int fcloseall(void)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fcloseall\n");
    while (open_files._chain)
    {
        rc = fclose(open_files._chain);
    }
    return rc;
}

/**
 * fclose first writes any dirty data in the buffer
 */
int fclose(FILE *stream)
{
    int rc = 0;
    FILE *f;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fclose %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fclose(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fclose(stream);
        }
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    /* write any pending data */
    if (ISFLAGSET(stream, _IO_CURRENTLY_PUTTING))
    {
        if (stream->_IO_write_ptr > stream->_IO_write_base)
        {
#if PVFS_STDIO_DEBUG
            fprintf(stderr,"fclose writing %d bytes to offset %d\n",
                    (int)(stream->_IO_write_ptr - stream->_IO_write_base),
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
            rc = write(stream->_fileno, stream->_IO_write_base,
                        stream->_IO_write_ptr - stream->_IO_write_base);
            if (rc == -1)   
            {
                SETFLAG(stream, _IO_ERR_SEEN);
                return -1;
            }
        }
    }
    if (!ISFLAGSET(stream, _IO_USER_BUF))
    {
        /* free the buffer */
        free(stream->_IO_buf_base);
    }
    if (!ISFLAGSET(stream, _IO_DELETE_DONT_CLOSE))
    {
        rc = close(stream->_fileno);
    }
    /* remove from chain */
    lock_stream(&open_files);
    if (open_files._chain == stream)
    {
        open_files._chain = stream->_chain;
    }
    else
    {
        for (f = open_files._chain; f && f->_chain != stream; f = f->_chain)
        if (f && f->_chain)
        {
            f->_chain = f->_chain->_chain;
        }
        /* was not on chain */
    }
    unlock_stream(&open_files);
    stream->_flags = 0;
    /* can stream be locked here ? */
    lock_fini_stream(stream);
    free(stream);
    return rc;
}

/**
 * fseek wrapper
 */
int fseek(FILE *stream, long offset, int whence)
{
    return fseek64(stream, (off64_t)offset, whence);
}

int fseeko(FILE *stream, off_t offset, int whence)
{
    return fseek64(stream, (off64_t)offset, whence);
}

int fseeko64(FILE *stream, off64_t offset, int whence)
{
    return fseek64(stream, (off64_t)offset, whence);
}

/** This is the main code for seeking on a stream
 * 
 *  If we seek a short distance within the current buffer
 *  we can just move the stream pointer.  Otherwise we
 *  have to clear the buffer, seek, and start fresh
 */
int fseek64(FILE *stream, const off64_t offset, int whence)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fseek64 %p %llx %d\n",
                    stream, offset, whence);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fseek64(stream, offset, whence);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fseek64(stream, offset, whence);
        }
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    /* if actually changing the position */
    if ((offset != 0L) || (whence != SEEK_CUR))
    {
        int64_t filepos, fileend;
        struct stat64 sbuf;
        filepos = lseek64(stream->_fileno, 0, SEEK_CUR);
        /* should fileend include stuff in write buffer ??? */
        rc = fstat64(stream->_fileno, &sbuf);
        if (rc < 0)
        {
            SETFLAG(stream, _IO_ERR_SEEN);
            rc = -1;
            goto exitout;
        }
        fileend = sbuf.st_size;
        /* figure out if we are only seeking within the */
        /* bounds of the current buffer to minimize */
        /* unneccessary reads/writes */
        if (whence == SEEK_CUR && ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
                (offset < stream->_IO_write_end - stream->_IO_write_ptr) &&
                (offset > stream->_IO_write_base - stream->_IO_write_ptr))
        {
            stream->_IO_write_ptr += offset;
            /* should we zero out buffer if past eof ??? */
            rc = 0;
            goto exitout;
        }
        if (whence == SEEK_CUR && !ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
                (offset < stream->_IO_read_end - stream->_IO_read_ptr) &&
                (offset > stream->_IO_read_base - stream->_IO_read_ptr))
        {
            stream->_IO_read_ptr += offset;
            rc = 0;
            goto exitout;
        }
        if (whence == SEEK_SET && ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
                (offset > filepos) && (offset < filepos +
                (stream->_IO_write_end - stream->_IO_write_base)))
        {
            stream->_IO_write_ptr += offset - filepos;
            /* should we zero out buffer if past eof ??? */
            rc = 0;
            goto exitout;
        }
        if (whence == SEEK_SET && !ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
                (offset < filepos) && (offset > filepos -
                (stream->_IO_read_end - stream->_IO_read_base)))
        {
            stream->_IO_read_ptr += offset - filepos;
            rc = 0;
            goto exitout;
        }
        if (whence == SEEK_END && ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
                ((fileend - offset) > filepos) &&
                ((fileend - offset) < filepos +
                (stream->_IO_write_end - stream->_IO_write_base)))
        {
            stream->_IO_write_ptr += (fileend - offset) - filepos;
            /* should we zero out buffer if past eof ??? */
            rc = 0;
            goto exitout;
        }
        if (whence == SEEK_END && !ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
                ((fileend - offset) < filepos) &&
                ((fileend - offset) > filepos -
                (stream->_IO_read_end - stream->_IO_read_base)))
        {
            stream->_IO_read_ptr += (fileend - offset) - filepos;
            rc = 0;
            goto exitout;
        }
        /* at this point the seek is beyond the current buffer */
        /* if we are in write mode write back the buffer */
        if (ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
            stream->_IO_write_ptr > stream->_IO_write_base)
        {
            /* write buffer back */
#if PVFS_STDIO_DEBUG
            fprintf(stderr,"fseek writing %d bytes to offset %d\n",
                    (int)(stream->_IO_write_ptr - stream->_IO_write_base),
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
            rc = write(stream->_fileno, stream->_IO_write_base,
                       stream->_IO_write_ptr - stream->_IO_write_base); 
            if (rc < 0)
            {
                SETFLAG(stream, _IO_ERR_SEEN);
                rc = -1;
                goto exitout;
            }
            /* reset write pointer */
            stream->_IO_write_ptr = stream->_IO_write_base;
        }
        else
        {
            /* in read mode simply clear the buffer */
            /* will force a read at next fread call */
            stream->_IO_read_end = stream->_IO_read_base;
            stream->_IO_read_ptr = stream->_IO_read_end;
        }
        rc = lseek64(stream->_fileno, offset, whence);
#if PVFS_STDIO_DEBUG
        fprintf(stderr,"fseek seeks to offset %d\n",
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
        if (rc < 0)
        {
            SETFLAG(stream, _IO_ERR_SEEN);
            rc = -1;
            goto exitout;
        }
        /* fseek returns 0 on success */
        rc = 0;
    }
exitout:
    /* successful call */
    unlock_stream(stream);
    CLEARFLAG(stream, _IO_EOF_SEEN);
    return rc;
}

/**
 * fsetpos wrapper
 */
int fsetpos(FILE *stream, const fpos_t *pos)
{
    fseek64(stream, (off64_t)(pos->__pos), SEEK_SET);
    return 0;
}

int fsetpos64(FILE *stream, const fpos64_t *pos)
{
    fseek64(stream, (off64_t)(pos->__pos), SEEK_SET);
    return 0;
}

/**
 * rewind wrapper
 */
void rewind(FILE *stream)
{
    fseek64(stream, 0L, SEEK_SET);
    CLEARFLAG(stream, _IO_ERR_SEEN);
}

/**
 * ftell wrapper
 */
long int ftell(FILE *stream)
{
    return (long)ftell64(stream);
}

off_t ftello(FILE *stream)
{
    return (off_t)ftell64(stream);
}

off64_t ftello64(FILE *stream)
{
    return (off64_t)ftell64(stream);
}

off64_t ftell64(FILE* stream)
{
    int64_t filepos;

    gossip_debug(GOSSIP_USRINT_DEBUG, "ftell64 %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.ftell64(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.ftell64(stream);
        }
        errno = EINVAL;
        return -1;
    }
    filepos = lseek64(stream->_fileno, 0, SEEK_CUR);
    if (ISFLAGSET(stream, _IO_CURRENTLY_PUTTING))
    {
        return filepos + (stream->_IO_write_ptr - stream->_IO_write_base);
    }
    else
    {
        return filepos - (stream->_IO_read_end - stream->_IO_read_ptr);
    }
}

/**
 * fgetpos wrapper
 */
int fgetpos(FILE *stream, fpos_t *pos)
{
    pos->__pos = ftell64(stream);
    return 0;
}

int fgetpos64(FILE *stream, fpos64_t *pos)
{
    pos->__pos = ftell64(stream);
    return 0;
}

/** forces a write back of potentially dirty buffer
 * 
 *  we don't have a dirty flag, so if user seeks
 *  ahead within the buffer then does a flush
 *  we will do an uncessary write
 */
int fflush(FILE *stream)
{
    int rc = 0;

#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fflush(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fflush(stream);
        }
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    rc = fflush_unlocked(stream);
    unlock_stream(stream);
    return rc;
}

int fflush_unlocked(FILE *stream)
{
    int rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fflush_unlocked %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fflush_unlocked(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fflush_unlocked(stream);
        }
        errno = EINVAL;
        return -1;
    }
    /* if we are in write mode write back the buffer */
    if (ISFLAGSET(stream, _IO_CURRENTLY_PUTTING) &&
        stream->_IO_write_ptr > stream->_IO_write_base)
    {
        /* write buffer back */
#if PVFS_STDIO_DEBUG
        fprintf(stderr,"fflush writing %d bytes to offset %d\n",
                    (int)(stream->_IO_write_ptr - stream->_IO_write_base),
                    (int)lseek(stream->_fileno, 0, SEEK_CUR));
#endif
        rc = write(stream->_fileno, stream->_IO_write_base,
                stream->_IO_write_ptr - stream->_IO_write_base); 
        if (rc < 0)
        {
            SETFLAG(stream, _IO_ERR_SEEN);
            return rc;
        }
        /* reset write pointer */
        stream->_IO_write_ptr = stream->_IO_write_base;
    }
    return 0;
}

/*
 * fputc wrapper
 */
int fputc(int c, FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fputc %c %p\n", c, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fputc(c, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fputc(c, stream);
        }
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    rc = fputc_unlocked(c, stream);
    unlock_stream(stream);
    return rc;
}

int fputc_unlocked(int c, FILE *stream)
{
    int rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fputc_unlocked %c %p\n", c, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fputc_unlocked(c, stream);
    }
#endif
    rc = fwrite_unlocked(&c, 1, 1, stream);
    if (ferror(stream))
    {
        return EOF;
    }
    return c;
}

/**
 * fputs writes up to a null char
 */
int fputs(const char *s, FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fputs %s %p\n", s, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fputs(s, stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fputs(s, stream);
        }
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    rc = fputs_unlocked(s, stream);
    unlock_stream(stream);
    return rc;
}

int fputs_unlocked(const char *s, FILE *stream)
{
    size_t len;
    int rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fputs_unlocked %s %p\n", s, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fputs_unlocked(s, stream);
    }
#endif
    if (!s)
    {
        errno = EINVAL;
        return EOF;
    }
    len = strlen(s);
    rc = fwrite_unlocked(s, len, 1, stream);
    if (ferror(stream))
    {
        return EOF;
    }
    return rc;
}

/**
 * putc wrapper
 */
int putc(int c, FILE *stream)
{
    return fputc(c, stream);
}

int putc_unlocked(int c, FILE *stream)
{
    return fputc_unlocked(c, stream);
}

/**
 * putchar wrapper
 */
int putchar(int c)
{
    return fputc(c, stdout);
}

int putchar_unlocked(int c)
{
    return fputc_unlocked(c, stdout);
}

/**
 * puts wrapper
 */
int puts(const char *s)
{
    int rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "puts %s\n", s);
#ifndef PVFS_STDIO_REDEFSTREAM
    init_stdio();
    rc = stdio_ops.puts(s);
    return rc;
#else
    rc = fputs(s, stdout);
    if (rc == EOF)
    {
        return EOF;
    }
    return fputs("\n", stdout);
#endif
}

/**
 * putw wrapper
 */
int putw(int wd, FILE *stream)
{
    int rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "putw %d %p\n", wd, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.putw(wd, stream);
    }
#endif
    rc = fwrite(&wd, sizeof(int), 1, stream);
    if (ferror(stream))
    {
        return EOF;
    }
    return rc;
}

/**
 * fgets reads up to size or a newline
 */
char *fgets(char *s, int size, FILE *stream)
{
    char *rc = NULL;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fgets %p %d %p\n", s, size, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc = stdio_ops.fgets(s, size, stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgets returns %s\n", s);
        return rc;
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fgets(s, size, stream);
        }
        errno = EINVAL;
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgets returns NULL\n");
        return NULL;
    }
    lock_stream(stream);
    rc = fgets_unlocked(s, size, stream);
    unlock_stream(stream); gossip_debug(GOSSIP_USRINT_DEBUG, "fgets returns %s\n", rc);
    return rc;
}

char *fgets_unlocked(char *s, int size, FILE *stream)
{
    char c, *p;
    int feo, fer;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fgets_unlocked %p %d %p\n",
                    s, size, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        s = stdio_ops.fgets_unlocked(s, size, stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgets_unlocked returns %s\n", s);
        return s;
    }
#endif
    if (!stream || !s || size < 1)
    {
        errno = EINVAL;
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgets_unlocked returns NULL\n");
        return NULL;
    }
    if (size == 1)
    {
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgets_unlocked returns \"\"\n");
        return s;
    }
    p = s;
    size--; /* for the trailing NULL char */
    do {
        *p++ = c = fgetc_unlocked(stream);
        /* reduce multiple func calls */
        feo = feof_unlocked(stream);
        fer = ferror_unlocked(stream);
    } while (--size && c != '\n' && !(feo || fer));
    /* if error or eof and read no chars */
    if (fer || (feo && p - s == 1))
    {
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgets_unlocked returns NULL\n");
        return NULL;
    }
    *p = 0; /* add null terminating char */
    gossip_debug(GOSSIP_USRINT_DEBUG, "fgets_unlocked returns %s\n", s);
    return s;
}

/**
 * fgetc wrapper
 */
int fgetc(FILE *stream)
{
    int rc;
    unsigned char ch;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc = stdio_ops.fgetc(stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc returns %d(%c)\n",
                        rc, (char)rc);
        return rc;
    }
#endif
    rc = fread(&ch, 1, 1, stream);
    if (ferror(stream) || feof(stream))
    {
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc returns %d\n", EOF);
        return EOF;
    }
    gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc returns %c\n", ch);
    return (int)ch;
}

int fgetc_unlocked(FILE *stream)
{
    int rc;
    char ch;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc_unlocked %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc = stdio_ops.fgetc_unlocked(stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc_unlocked returns %d(%c)\n",
                        rc, (char)rc);
        return rc;
    }
#endif
    rc = fread_unlocked(&ch, 1, 1, stream);
    if (ferror_unlocked(stream) || feof_unlocked(stream))
    {
        gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc_unlocked returns %d\n", EOF);
        return EOF;
    }
    gossip_debug(GOSSIP_USRINT_DEBUG, "fgetc_unlocked returns %c\n", ch);
    return (int)ch;
}

/**
 * getc wrapper
 */
int getc(FILE *stream)
{
    return fgetc(stream);
}

int getc_unlocked(FILE *stream)
{
    return fgetc_unlocked(stream);
}

/**
 * getchar wrapper
 */
int getchar(void)
{
    return fgetc(stdin);
}

int getchar_unlocked(void)
{
    return fgetc_unlocked(stdin);
}

/**
 * getw wrapper
 *
 * not sure if feof should return an EOF or not
 */
int getw(FILE *stream)
{
    int rc, wd;

    gossip_debug(GOSSIP_USRINT_DEBUG, "getw %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.getw(stream);
    }
#endif
    rc = fread(&wd, sizeof(int), 1, stream);
    if (ferror(stream) || feof(stream))
    {
        return EOF;
    }
    return wd;
}

/**
 * gets
 */
char *gets(char * s)
{
#ifdef PVFS_STDIO_REDEFSTREAM
    char c, *p;
#endif

    gossip_debug(GOSSIP_USRINT_DEBUG, "gets %p\n", s);
#ifndef PVFS_STDIO_REDEFSTREAM
    init_stdio();
    return stdio_ops.gets(s);
#else
    if (!s)
    {
        errno = EINVAL;
        return NULL;
    }
    p = s;
    do {
        *p++ = c = fgetc(stdin);
    } while (c != '\n' && !feof(stdin) && !ferror(stdin));
    if (ferror(stdin) || ((p = s + 1) && feof(stdin)))
    {
        return NULL;
    }
    if (!feof(stdin))
    {
        *(p-1) = 0; /* replace terminating char with null */
    }
    return s;
#endif
}

/**
 * getline
 */
ssize_t getline(char **lnptr, size_t *n, FILE *stream)
{
    return __getdelim(lnptr, n, '\n', stream);
}

ssize_t getdelim(char **lnptr, size_t *n, int delim, FILE *stream)
{
    return __getdelim(lnptr, n, delim, stream);
}

ssize_t __getdelim(char **lnptr, size_t *n, int delim, FILE *stream)
{
    int i = 0;
    char c, *p;

    gossip_debug(GOSSIP_USRINT_DEBUG, "getdelim %p, %d, %d, %p\n", 
                    lnptr, *n, delim, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.getdelim(lnptr, n, delim, stream);
    }
#endif
    if (!stream || !n)
    {
        errno = EINVAL;
        return -1;
    }
    if (!*lnptr)
    {
        *n = 256;
        *lnptr = (char *)malloc(*n);
        if (!*lnptr)
        {
            return -1;
        }
    }
    p = *lnptr;
    do {
        if (i + 1 >= *n) /* need space for next char and null terminator */
        {
            *n += 256; /* spec gives no guidance on fit of allocated space */
            *lnptr = realloc(*lnptr, *n);
            if (!*lnptr)
            {
                return -1;
            }
            p = *lnptr + i;
        }
        *p++ = c = fgetc(stream);
        i++;
    } while (c != delim && !feof(stream) && !ferror(stream));
    if (ferror(stream) || feof(stream))
    {
        return -1;
    }
    *p = 0; /* null termintor */
    return i;
}

/**
 * ungetc wrapper
 *
 * TODO: at the moment this will not unget beyond the current
 *       buffer - needs a better implementation using the backup
 *       buffer area _IO_save_base, _IO_save_end, _IO_backup_base
 */
int ungetc(int c, FILE *stream)
{
    int64_t rc;

    gossip_debug(GOSSIP_USRINT_DEBUG, "ungetc %d, %p\n", 
                    c, stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.ungetc(c, stream);
    }
#endif
    rc = fseek64(stream, -1L, SEEK_CUR);
    if (rc < 0)
    {
        return EOF;
    }
    return c;
}

/**
 * We don't need any flavor of sprintf or sscanf
 * they don't do IO on a stream
 */
#if 0
sprintf, snprintf, vsprintf, vsnprintf, asprintf, vasprintfm
sscanf, vsscanf
#endif

/**
 * dprintf wrapper
 */
int dprintf(int fd, const char *format, ...)
{
    size_t len;
    va_list ap;

    va_start(ap, format);
    len = vdprintf(fd, format, ap);
    va_end(ap);
    return len;
}

/**
 * vdprintf 
 */
int vdprintf(int fd, const char *format, va_list ap)
{
    char *buf;
    int len, rc = 0;

    len = vasprintf(&buf, format, ap);
    if (len < 0)
    {
        return len;
    }
    if (len > 0 && buf)
    {
        rc = write(fd, buf, len);
    }
    if (buf)
    {
        free(buf);
    }
    return rc;
}

/**
 * vfprintf using a var arg list
 */
int vfprintf(FILE *stream, const char *format, va_list ap)
{
    char *buf;
    int len, rc = 0;

#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.vfprintf(stream, format, ap);
    }
#endif
    len = vasprintf(&buf, format, ap);
    if (len < 0)
    {
        return len;
    }
    if (len > 0 && buf)
    {
        rc = fwrite(buf, len, 1, stream);
    }
    if (buf)
    {
        free(buf);
    }
    return rc;
}

/**
 * fprintf wrapper
 */
int vprintf(const char *format, va_list ap)
{
    return vfprintf(stdout, format, ap);
}

/**
 * fprintf wrapper
 */
int fprintf(FILE *stream, const char *format, ...)
{
    size_t len;
    va_list ap;

    va_start(ap, format);
    len = vfprintf(stream, format, ap);
    va_end(ap);
    return len;
}

/**
 * printf wrapper
 */
int printf(const char *format, ...)
{
    size_t len;
    va_list ap;

    va_start(ap, format);
    len = vfprintf(stdout, format, ap);
    va_end(ap);
    return len;
}

/**
 * perror
 */
void perror(const char *s)
{
#ifndef PVFS_STDIO_REDEFSTREAM
    init_stdio();
    stdio_ops.perror(s);
    return;
#else
    char *msg;
    if (s && *s)
    {
        fwrite(s, strlen(s), 1, stderr);
    }
    msg = strerror(errno);
    fwrite(msg, strlen(msg), 1, stderr);
    fwrite("\n", 1, 1, stderr);
#endif
}

#if 0
/* TODO: These are not implemented yet */

scanf()
{
}

fscanf()
{
}

vfscanf()
{
}

#endif

/**
 * Stdio utilitie to clear error and eof for a stream
 */
void clearerr (FILE *stream)
{
    gossip_debug(GOSSIP_USRINT_DEBUG, "clearerr %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.clearerr(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            stdio_ops.clearerr(stream);
            return;
        }
        return;
    }
    lock_stream(stream);
    CLEARFLAG(stream, _IO_ERR_SEEN);
    CLEARFLAG(stream, _IO_EOF_SEEN);
    unlock_stream(stream);
}

void clearerr_unlocked (FILE *stream)
{
    gossip_debug(GOSSIP_USRINT_DEBUG, "clearerr_unlocked %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.clearerr_unlocked(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            stdio_ops.clearerr_unlocked(stream);
            return;
        }
        return;
    }
    CLEARFLAG(stream, _IO_ERR_SEEN);
    CLEARFLAG(stream, _IO_EOF_SEEN);
}

/**
 * Stdio utilitie to check if a stream is at EOF
 */
int feof (FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "feof %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc = stdio_ops.feof(stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "feof returns %d\n", rc);
        return rc;
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.feof(stream);
        }
        errno = EINVAL;
        gossip_debug(GOSSIP_USRINT_DEBUG, "feof returns %d\n", -1);
        return -1;
    }
    lock_stream(stream);
    rc = ISFLAGSET(stream, _IO_EOF_SEEN);
    unlock_stream(stream);
    gossip_debug(GOSSIP_USRINT_DEBUG, "feof returns %d\n", rc);
    return rc;
}

int feof_unlocked (FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "feof_unlocked %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc = stdio_ops.feof_unlocked(stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "feof_unlocked returns %d\n", rc);
        return rc;
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.feof_unlocked(stream);
        }
        errno = EBADF;
        gossip_debug(GOSSIP_USRINT_DEBUG, "feof_unlocked returns %d\n", -1);
        return -1;
    }
    rc = ISFLAGSET(stream, _IO_EOF_SEEN);
    gossip_debug(GOSSIP_USRINT_DEBUG, "feof_unlocked returns %d\n", rc);
    return rc;
}

/**
 * Stdio utilitie to check for error on a stream
 */
int ferror (FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "ferror %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc = stdio_ops.ferror(stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "ferror returns %d\n", rc);
        return rc;
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.ferror(stream);
        }
        errno = EINVAL;
        gossip_debug(GOSSIP_USRINT_DEBUG, "ferror returns %d\n", -1);
        return -1;
    }
    lock_stream(stream);
    rc = ISFLAGSET(stream, _IO_ERR_SEEN);
    unlock_stream(stream);
    gossip_debug(GOSSIP_USRINT_DEBUG, "ferror returns %d\n", rc);
    return rc;
}

int ferror_unlocked (FILE *stream)
{
    int rc = 0;
    gossip_debug(GOSSIP_USRINT_DEBUG, "ferror_unlocked %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        rc =stdio_ops.ferror_unlocked(stream);
        gossip_debug(GOSSIP_USRINT_DEBUG, "ferror_unlocked returns %d\n", rc);
        return rc;
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.ferror_unlocked(stream);
        }
        errno = EBADF;
        gossip_debug(GOSSIP_USRINT_DEBUG, "ferror_unlocked returns %d\n", -1);
        return -1;
    }
    gossip_debug(GOSSIP_USRINT_DEBUG, "ferror_unlocked returns %d\n", rc);
    return ISFLAGSET(stream, _IO_ERR_SEEN);
}

/**
 * Stdio utilitie to get file descriptor from a stream
 */
int fileno (FILE *stream)
{
    int rc = 0;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fileno %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fileno(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fileno(stream);
        }
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    rc = stream->_fileno;
    unlock_stream(stream);
    return rc;
}

int fileno_unlocked (FILE *stream)
{
    gossip_debug(GOSSIP_USRINT_DEBUG, "fileno_unlocked %p\n", stream);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.fileno_unlocked(stream);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.fileno_unlocked(stream);
        }
        errno = EBADF;
        return -1;
    }
    return stream->_fileno;
}

/** stdio function to delete a file
 *
 */
int remove (const char *path)
{
    int rc;
    struct stat buf;

    gossip_debug(GOSSIP_USRINT_DEBUG, "remove %s\n", path);
    rc = stat(path, &buf);
    if (S_ISDIR(buf.st_mode))
        return rmdir (path);
    return unlink (path);
}

/**
 *  setbuf wrapper
 */
void setbuf (FILE *stream, char *buf)
{
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, BUFSIZ);
}

/**
 *  setbuffer wrapper
 */
void setbuffer (FILE *stream, char *buf, size_t size)
{
    setvbuf(stream, buf, buf ? _IOFBF : _IONBF, size);
}

/**
 *  setlinbuf wrapper
 */
void setlinebuf (FILE *stream)
{
    setvbuf(stream, (char *)NULL, _IOLBF, 0);
}

/**
 *
 * This should only be called on a stream that has been opened
 * but not used so we can assume any exiting buff is not dirty
 */
int setvbuf (FILE *stream, char *buf, int mode, size_t size)
{
    gossip_debug(GOSSIP_USRINT_DEBUG, "setvbuf %p %p %d %d\n", 
                    stream, buf, mode, size);
#ifndef PVFS_STDIO_REDEFSTREAM
    if (stream == stdin || stream == stdout || stream == stderr)
    {
        init_stdio();
        return stdio_ops.setvbuf(stream, buf, mode, size);
    }
#endif
    if (!stream || !ISMAGICSET(stream, _P_IO_MAGIC))
    {
        if (stream && ISMAGICSET(stream, _IO_MAGIC))
        {
            init_stdio();
            return stdio_ops.setvbuf(stream, buf, mode, size);
        }
        errno = EINVAL;
        return -1;
    }
    if ((stream->_IO_read_end != stream->_IO_buf_base) ||
        (stream->_IO_write_ptr != stream->_IO_buf_base))
    {
        /* fread or fwrite has been called */
        errno = EINVAL;
        return -1;
    }
    lock_stream(stream);
    switch (mode)
    {
    case _IOFBF : /* full buffered */
        /* this is the default */
        break;
    case _IOLBF : /* line buffered */
        SETFLAG(stream, _IO_LINE_BUF); /* TODO: This is not implemented */
        break;
    case _IONBF : /* not buffered */
        SETFLAG(stream, _IO_UNBUFFERED); /* TODO: This is not implemented */
        break;
    default :
        errno = EINVAL;
        unlock_stream(stream);
        return -1;
    }
    if (buf && size > 0)
    {
        SETFLAG(stream, _IO_USER_BUF);
        free(stream->_IO_buf_base);
        stream->_IO_buf_base   = buf;
        stream->_IO_buf_end    = stream->_IO_buf_base + size;
        stream->_IO_read_base  = stream->_IO_buf_base;
        stream->_IO_read_ptr   = stream->_IO_buf_base;
        stream->_IO_read_end   = stream->_IO_buf_base;
        stream->_IO_write_base = stream->_IO_buf_base;
        stream->_IO_write_ptr  = stream->_IO_buf_base;
        stream->_IO_write_end  = stream->_IO_buf_end;
    }
    unlock_stream(stream);
    return 0;
}

/**
 * mkdtemp makes a temp dir and returns an fd 
 */
char *mkdtemp(char *template)
{
    int fd;
    int len;
    int rnum;
    int try;

    if (!template)
    {
        errno = EINVAL;
        return NULL;
    }
    len = strlen(template);
    if (!strncmp(&template[len-6],"XXXXXX",6))
    {
        errno = EINVAL;
        return NULL;
    }
    for(try = 0; try < MAXTRIES; try++)
    {
        rnum = PINT_random() % 1000000;
        sprintf(&template[len-6],"%06d", rnum);
        fd = mkdir(template, 0700);
        if (fd < 0)
        {
            if (errno == EEXIST)
            {
                continue;
            }
            return NULL;
        }
    }
    return template;
}

/**
 * mkstemp makes a temp file and returns an fd 
 */
int mkstemp(char *template)
{
    int fd;
    int len;
    int rnum;
    int try;

    if (!template)
    {
        errno = EINVAL;
        return -1;
    }
    len = strlen(template);
    if (!strncmp(&template[len-6],"XXXXXX",6))
    {
        errno = EINVAL;
        return -1;
    }
    for(try = 0; try < MAXTRIES; try++)
    {
        rnum = PINT_random() % 1000000;
        sprintf(&template[len-6],"%06d", rnum);
        fd = open(template, O_RDWR|O_EXCL|O_CREAT, 0600);
        if (fd < 0)
        {
            if (errno == EEXIST)
            {
                continue;
            }
            return -1;
        }
    }
    return fd;
}

/**
 * tmpfile makes a temp file and returns a stream 
 */
FILE *tmpfile(void)
{
    char *template = "/tmp/tmpfileXXXXXX";
    int fd;
    fd = mkstemp(template);
    if (fd < 0)
    {
        return NULL;
    }
    return fdopen(fd, "r+");
}

/**
 * opendir opens a directory as a stream
 */
DIR *opendir (const char *name)
{
    int fd;

    gossip_debug(GOSSIP_USRINT_DEBUG, "opendir %s\n", name);
    if(!name)
    {
        errno = EINVAL;
        return NULL;
    }
    fd = open(name, O_RDONLY|O_DIRECTORY);
    if (fd < 0)
    {
        return NULL;
    }
    return fdopendir(fd);
}

/**
 * creates a stream for an already open directory
 */
DIR *fdopendir (int fd)
{
    DIR *dstr;

    gossip_debug(GOSSIP_USRINT_DEBUG, "fdopendir %d\n", fd);
    dstr = (DIR *)malloc(sizeof(DIR));
    if (dstr == NULL)
    {
        return NULL;
    }
    memset(dstr, 0, sizeof(DIR));
    SETMAGIC(dstr, DIRSTREAM_MAGIC);
    dstr->fileno = fd;
    dstr->buf_base = (char *)malloc(DIRBUFSIZE);
    if (dstr->buf_base == NULL)
    {
        dstr->_flags = 0;
        free(dstr);
        return NULL;
    }
    dstr->buf_end = dstr->buf_base + DIRBUFSIZE;
    dstr->buf_act = dstr->buf_base;
    dstr->buf_ptr = dstr->buf_base;
    return dstr;
}

/**
 * returns the file descriptor for a directory stream
 */
int dirfd (DIR *dir)
{
    gossip_debug(GOSSIP_USRINT_DEBUG, "dirfd %p\n", dir);
    if (!dir || !ISMAGICSET(dir, DIRSTREAM_MAGIC))
    {
        errno = EBADF;
        return -1;
    }
    return dir->fileno;
}

/**
 * readdir wrapper
 */
struct dirent *readdir (DIR *dir)
{
    struct dirent64 *de64;

    gossip_debug(GOSSIP_USRINT_DEBUG, "readdir %p\n", dir);
    de64 = readdir64(dir);
    if (de64 == NULL)
    {
        return NULL;
    }
    /* linux hard defines d_name to 256 bytes */
    /* if others don't should replace with a define */
    memset(&dir->de, 0, sizeof(dir->de));
    memcpy(dir->de.d_name, de64->d_name, 256);
    dir->de.d_ino = de64->d_ino;
    /* these are system specific fields from the dirent */
#ifdef _DIRENT_HAVE_D_NAMELEN
    dir->de.d_namelen = strnlen(de64->d_name, 256);
#endif
#ifdef _DIRENT_HAVE_D_OFF
    dir->de.d_off = de64->d_off;
#endif
#ifdef _DIRENT_HAVE_D_RECLEN
    dir->de.d_reclen = de64->d_reclen;
#endif
#ifdef _DIRENT_HAVE_D_TYPE
    dir->de.d_type = de64->d_type;
#endif
    return &dir->de;
}

/**
 * reads a single dirent64 in buffered mode from a stream
 *
 * getdents is not defined in libc, though it is a linux
 * system call and we define it in the usr lib
 */

int getdents(int fd, struct dirent *buf, size_t size);
int getdents64(int fd, struct dirent64 *buf, size_t size);

struct dirent64 *readdir64 (DIR *dir)
{
    struct dirent64 *rval;

    gossip_debug(GOSSIP_USRINT_DEBUG, "readdir64 %p\n", dir);
    if (!dir || !ISMAGICSET(dir, DIRSTREAM_MAGIC))
    {
        errno = EBADF;
        return NULL;
    }
    if (dir->buf_ptr >= dir->buf_act)
    {
        int bytes_read;
        /* read a block of dirent64s into the buffer */
        bytes_read = getdents64(dir->fileno, (struct dirent64 *)dir->buf_base,
                               (dir->buf_end - dir->buf_base));
        if (bytes_read <= 0)
        {
            return NULL; /* EOF if errno == 0 */
        }
        dir->buf_act = dir->buf_base + bytes_read;
        dir->buf_ptr = dir->buf_base;
    }
    rval = (struct dirent64 *)dir->buf_ptr;
#ifdef _DIRENT_HAVE_D_RECLEN
    dir->buf_ptr += rval->d_reclen;
#else
    dir->buf_ptr += sizeof(struct dirent64);
#endif
    return rval;
}

/**
 * rewinds a directory stream
 */
void rewinddir (DIR *dir)
{
    off64_t filepos;
 
    gossip_debug(GOSSIP_USRINT_DEBUG, "rewinddir %p\n", dir);
    if (!dir || !ISMAGICSET(dir, DIRSTREAM_MAGIC))
    {
        errno = EBADF;
        return;
    }
    filepos = lseek64(dir->fileno, 0, SEEK_CUR);
    if ((filepos - (dir->buf_act - dir->buf_base)) == 0)
    {
        dir->buf_ptr = dir->buf_base;
    }
    else
    {
        dir->buf_act = dir->buf_base;
        dir->buf_ptr = dir->buf_base;
        lseek64(dir->fileno, 0, SEEK_SET);
    }
}

/**
 * seeks in a directory stream
 */
void seekdir (DIR *dir, off_t offset)
{
    off64_t filepos;

    gossip_debug(GOSSIP_USRINT_DEBUG, "seekdir %p\n", dir);
    if (!dir || !ISMAGICSET(dir, DIRSTREAM_MAGIC))
    {
        errno = EBADF;
        return;
    }
    filepos = lseek64(dir->fileno, 0, SEEK_CUR);
    if ((filepos - (dir->buf_act - dir->buf_base)) <= offset &&
        filepos >= offset)
    {
        dir->buf_ptr = dir->buf_act - (filepos - offset);
    }
    else
    {
        dir->buf_act = dir->buf_base;
        dir->buf_ptr = dir->buf_base;
        lseek64(dir->fileno, offset, SEEK_SET);
    }
}

/**
 * returns current position in a direcotry stream
 */
off_t telldir (DIR *dir)
{
    off64_t filepos;

    gossip_debug(GOSSIP_USRINT_DEBUG, "telldir %p\n", dir);
    if (!dir || !ISMAGICSET(dir, DIRSTREAM_MAGIC))
    {
        errno = EBADF;
        return -1;
    }
    filepos = lseek64(dir->fileno, 0, SEEK_CUR);
    if (filepos == -1)
    {
        return -1;
    }
    return filepos - (dir->buf_act - dir->buf_ptr);
}

/**
 * closes a direcotry stream
 */
int closedir (DIR *dir)
{
    gossip_debug(GOSSIP_USRINT_DEBUG, "closedir %p\n", dir);
    if (!dir || !ISMAGICSET(dir, DIRSTREAM_MAGIC))
    {
        errno = EBADF;
        return -1;
    }
    free(dir->buf_base);
    dir->_flags = 0;
    free(dir);
    return 0;
}

#ifdef PVFS_SCANDIR_VOID
int scandir (const char *dir,
             struct dirent ***namelist,
             int(*filter)(const struct dirent *),
             int(*compar)(const void *,
                          const void *))
#else
int scandir (const char *dir,
             struct dirent ***namelist,
             int(*filter)(const struct dirent *),
             int(*compar)(const struct dirent **,
                          const struct dirent **))
#endif
{
    struct dirent *de;
    DIR *dp;
    int len, i, rc;
    int asz = ASIZE;

    gossip_debug(GOSSIP_USRINT_DEBUG, "scandir %p\n", dir);
    /* open directory */
    dp = opendir(dir);
    /* allocate namelist */
    *namelist = (struct dirent **)malloc(asz * sizeof(struct dirent *));
    if (!*namelist)
    {
        return -1;
    }
    /* loop through the dirents */
    for(i = 0, de = readdir(dp); de; i++, de = readdir(dp))
    {
        if (!filter || filter(de))
        {
            if (i >= asz)
            {
                struct dirent **darray;
                /* ran out of space, realloc */
                darray = (struct dirent **)realloc(*namelist, asz + ASIZE);
                if (!darray)
                {
                    int j;
                    for (j = 0; j < i; j++)
                    {
                        free(*namelist[j]);
                    }
                    free(*namelist);
                    return -1;
                }
                *namelist = darray;
                asz += ASIZE;
            }
            /* find the size of this entry */
            len = strnlen((*namelist)[i]->d_name, NAME_MAX + 1) +
                           sizeof(struct dirent);
            /* add to namelist */
            *namelist[i] = (struct dirent *)malloc(len);
            memcpy((*namelist)[i], de, len);
        }
    }
    /* now sort entries */
    qsort(*namelist, i, sizeof(struct dirent *), (__compar_fn_t)compar);
    rc = closedir(dp);
    if (rc == -1)
    {
        return -1;
    }
    return i;
}

/**
 * 64 bit version of scandir
 *
 * TODO: Would prefer not to copy code - modify to a generic version
 * and then call from two wrapper versions would be beter
 * pass in a flag to control the copy of the dirent into the array
 */
#ifdef PVFS_SCANDIR_VOID
int scandir64 (const char *dir,
               struct dirent64 ***namelist,
               int(*filter)(const struct dirent64 *),
               int(*compar)(const void *,
                            const void *))
#else
int scandir64 (const char *dir,
               struct dirent64 ***namelist,
               int(*filter)(const struct dirent64 *),
               int(*compar)(const struct dirent64 **,
                            const struct dirent64 **))
#endif
{
    struct dirent64 *de;
    DIR *dp;
    int len, i, rc;
    int asz = ASIZE;

    gossip_debug(GOSSIP_USRINT_DEBUG, "scandir64 %p\n", dir);
    /* open directory */
    dp = opendir(dir);
    /* allocate namelist */
    *namelist = (struct dirent64 **)malloc(asz * sizeof(struct dirent64 *));
    if (!*namelist)
    {
        return -1;
    }
    /* loop through the dirents */
    for(i = 0, de = readdir64(dp); de; i++, de = readdir64(dp))
    {
        if (!filter || filter(de))
        {
            if (i >= asz)
            {
                struct dirent64 **darray;
                /* ran out of space, realloc */
                darray = (struct dirent64 **)realloc(*namelist, asz + ASIZE);
                if (!darray)
                {
                    int j;
                    for (j = 0; j < i; j++)
                    {
                        free(*namelist[j]);
                    }
                    free(*namelist);
                    return -1;
                }
                *namelist = darray;
                asz += ASIZE;
            }
            /* find the size of this entry */
            len = strnlen((*namelist)[i]->d_name, NAME_MAX + 1) +
                           sizeof(struct dirent64);
            /* add to namelist */
            (*namelist)[i] = (struct dirent64 *)malloc(len);
            memcpy((*namelist)[i], de, len);
        }
    }
    /* now sort entries */
    qsort(*namelist, i, sizeof(struct dirent64 *), (__compar_fn_t)compar);
    rc = closedir(dp);
    if (rc == -1)
    {
        return -1;
    }
    return i;
}

static void init_stdio(void)
{
    static int init_flag = 0;
    static gen_mutex_t initlock = GEN_MUTEX_INITIALIZER;
    /* if we've already done this bail right away */
    if (init_flag)
    {
        return;
    }
    gen_mutex_lock(&initlock);
    /* don't let more than one thread initialize */
    if (init_flag)
    {
        gen_mutex_unlock(&initlock);        
        return;
    }
    /* init open file chain */
    lock_init_stream(&open_files);
    /* init pointers to glibc stdio calls */
    stdio_ops.fopen = dlsym(RTLD_NEXT, "fopen" );
    stdio_ops.fdopen = dlsym(RTLD_NEXT, "fdopen" );
    stdio_ops.freopen = dlsym(RTLD_NEXT, "freopen" );
    stdio_ops.fwrite = dlsym(RTLD_NEXT, "fwrite" );
    stdio_ops.fwrite_unlocked = dlsym(RTLD_NEXT, "fwrite_unlocked" );
    stdio_ops.fread  = dlsym(RTLD_NEXT, "fread" );
    stdio_ops.fread_unlocked = dlsym(RTLD_NEXT, "fread_unlocked" );
    stdio_ops.fclose = dlsym(RTLD_NEXT, "fclose" );
    stdio_ops.fseek = dlsym(RTLD_NEXT, "fseek" );
    stdio_ops.fseek64 = dlsym(RTLD_NEXT, "fseek64" );
    stdio_ops.fsetpos = dlsym(RTLD_NEXT, "fsetpos" );
    stdio_ops.rewind = dlsym(RTLD_NEXT, "rewind" );
    stdio_ops.ftell = dlsym(RTLD_NEXT, "ftell" );
    stdio_ops.ftell64 = dlsym(RTLD_NEXT, "ftell64" );
    stdio_ops.fgetpos = dlsym(RTLD_NEXT, "fgetpos" );
    stdio_ops.fflush  = dlsym(RTLD_NEXT, "fflush" );
    stdio_ops.fflush_unlocked = dlsym(RTLD_NEXT, "fflush_unlocked" );
    stdio_ops.fputc  = dlsym(RTLD_NEXT, "fputc" );
    stdio_ops.fputc_unlocked = dlsym(RTLD_NEXT, "fputc_unlocked" );
    stdio_ops.fputs  = dlsym(RTLD_NEXT, "fputs" );
    stdio_ops.fputs_unlocked = dlsym(RTLD_NEXT, "fputs_unlocked" );
    stdio_ops.putc  = dlsym(RTLD_NEXT, "putc" );
    stdio_ops.putc_unlocked = dlsym(RTLD_NEXT, "putc_unlocked" );
    stdio_ops.putchar  = dlsym(RTLD_NEXT, "putchar" );
    stdio_ops.putchar_unlocked = dlsym(RTLD_NEXT, "putchar_unlocked" );
    stdio_ops.puts = dlsym(RTLD_NEXT, "puts" );
    stdio_ops.putw = dlsym(RTLD_NEXT, "putw" );
    stdio_ops.fgets = dlsym(RTLD_NEXT, "fgets" );
    stdio_ops.fgetc = dlsym(RTLD_NEXT, "fgetc" );
    stdio_ops.getc = dlsym(RTLD_NEXT, "getc" );
    stdio_ops.getc_unlocked = dlsym(RTLD_NEXT, "getc_unlocked" );
    stdio_ops.getchar = dlsym(RTLD_NEXT, "getchar" );
    stdio_ops.getchar_unlocked = dlsym(RTLD_NEXT, "getchar_unlocked" );
    stdio_ops.getw = dlsym(RTLD_NEXT, "getw" );
    stdio_ops.gets = dlsym(RTLD_NEXT, "gets" );
    stdio_ops.getdelim = dlsym(RTLD_NEXT, "getdelim" );
    stdio_ops.ungetc = dlsym(RTLD_NEXT, "ungetc" );
    stdio_ops.vfprintf = dlsym(RTLD_NEXT, "vfprintf" );
    stdio_ops.vprintf = dlsym(RTLD_NEXT, "vprintf" );
    stdio_ops.fprintf = dlsym(RTLD_NEXT, "fprintf" );
    stdio_ops.printf = dlsym(RTLD_NEXT, "printf" );
    stdio_ops.perror = dlsym(RTLD_NEXT, "perror" );
    stdio_ops.fscanf = dlsym(RTLD_NEXT, "fscanf" );
    stdio_ops.scanf = dlsym(RTLD_NEXT, "scanf" );
    stdio_ops.clearerr  = dlsym(RTLD_NEXT, "clearerr" );
    stdio_ops.clearerr_unlocked  = dlsym(RTLD_NEXT, "clearerr_unlocked" );
    stdio_ops.feof  = dlsym(RTLD_NEXT, "feof" );
    stdio_ops.feof_unlocked  = dlsym(RTLD_NEXT, "feof_unlocked" );
    stdio_ops.ferror  = dlsym(RTLD_NEXT, "ferror" );
    stdio_ops.ferror_unlocked  = dlsym(RTLD_NEXT, "ferror_unlocked" );
    stdio_ops.fileno  = dlsym(RTLD_NEXT, "fileno" );
    stdio_ops.fileno_unlocked  = dlsym(RTLD_NEXT, "fileno_unlocked" );
    stdio_ops.remove  = dlsym(RTLD_NEXT, "remove" );
    stdio_ops.setbuf  = dlsym(RTLD_NEXT, "setbuf" );
    stdio_ops.setbuffer  = dlsym(RTLD_NEXT, "setbuffer" );
    stdio_ops.setlinebuf  = dlsym(RTLD_NEXT, "setlinebuf" );
    stdio_ops.setvbuf  = dlsym(RTLD_NEXT, "setvbuf" );
    stdio_ops.mkdtemp = dlsym(RTLD_NEXT, "mkdtemp" );
    stdio_ops.mkstemp = dlsym(RTLD_NEXT, "mkstemp" );
    stdio_ops.tmpfile = dlsym(RTLD_NEXT, "tmpfile" );
    stdio_ops.opendir  = dlsym(RTLD_NEXT, "opendir" );
    stdio_ops.fdopendir  = dlsym(RTLD_NEXT, "fdopendir" );
    stdio_ops.dirfd  = dlsym(RTLD_NEXT, "dirfd" );
    stdio_ops.readdir  = dlsym(RTLD_NEXT, "readdir" );
    stdio_ops.readdir64  = dlsym(RTLD_NEXT, "readdir64" );
    stdio_ops.rewinddir  = dlsym(RTLD_NEXT, "rewinddir" );
    stdio_ops.seekdir  = dlsym(RTLD_NEXT, "seekdir" );
    stdio_ops.telldir  = dlsym(RTLD_NEXT, "telldir" );
    stdio_ops.closedir  = dlsym(RTLD_NEXT, "closedir" );
    stdio_ops.scandir  = dlsym(RTLD_NEXT, "scandir" );
    stdio_ops.scandir64  = dlsym(RTLD_NEXT, "scandir64" );
    
    /* Finish */    
    init_flag = 1;
    gen_mutex_unlock(&initlock);
};

/*
 * Local variables:
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */