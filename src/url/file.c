#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
//#include <io.h>
#include "url.h"
#include "av_string.h"
#include "common.h"

/**
 *  open file.
 *
 * @param puc pointer to an URLContext
 * @param filename filename
 * @param flags flags which control how  to opened file
 * @return 0 in case of success, a negative value corresponding to an
 * AVERROR code in case of failure
 */
static int file_open(URLContext *h, const char *filename, int flags)
{
    int access;
    int fd;

    av_strstart(filename, "file:", &filename);

    if (flags & AVIO_FLAG_WRITE && flags & AVIO_FLAG_READ) {
        access = O_CREAT | O_TRUNC | O_RDWR;
    } else if (flags & AVIO_FLAG_WRITE) {
        access = O_CREAT | O_TRUNC | O_WRONLY;
    } else {
        access = O_RDONLY;
    }
/*	
#ifdef O_BINARY
    access |= O_BINARY;
#endif
*/
    fd = open(filename, access, 0666);
    if (fd == -1)
        return AVERROR(errno);
    h->priv_data = (void *) (intptr_t) fd;
    return 0;
}

/**
 * Close file
 *
 * @return a negative value if an error condition occurred, 0
 * otherwise
 */
static int file_close(URLContext *h)
{
    int fd = (intptr_t) h->priv_data;
    return close(fd);
}

/**
 * Read up to size bytes from the file by h, and store
 * the read bytes in buf.
 *
 * @param puc pointer to an URLContext
 * @param buf pointer to the buf 
 * @param size buf size
 * @return The number of bytes actually read
 */
static int file_read(URLContext *h, unsigned char *buf, int size)
{
    int fd = (intptr_t) h->priv_data;
    return read(fd, buf, size);
}

/**
 * Write size bytes from buf to the file by h.
 *
 * @param puc pointer to an URLContext
 * @param buf pointer to the buf 
 * @param size buf size
 * @return the number of bytes actually written, or a negative value
 * corresponding to an AVERROR code in case of failure
 */
static int file_write(URLContext *h, const unsigned char *buf, int size)
{
    int fd = (intptr_t) h->priv_data;
    return write(fd, buf, size);
}


/* XXX: use llseek */
static int64_t file_seek(URLContext *h, int64_t pos, int whence)
{
    int fd = (intptr_t) h->priv_data;
    if (whence == AVSEEK_SIZE) {
        struct stat st;
        int ret = fstat(fd, &st);
        return ret < 0 ? AVERROR(errno) : st.st_size;
    }
    return lseek(fd, pos, whence);
}

static int file_get_handle(URLContext *h)
{
    return (intptr_t) h->priv_data;
}

static int file_check(URLContext *h, int mask)
{
    struct stat st;
    int ret = stat(h->filename, &st);
    if (ret < 0)
        return AVERROR(errno);

    ret |= st.st_mode&S_IRUSR ? mask&AVIO_FLAG_READ  : 0;
    ret |= st.st_mode&S_IWUSR ? mask&AVIO_FLAG_WRITE : 0;

    return ret;
}

URLProtocol ff_file_protocol = {
    .name				= "file",
    .url_open 			= file_open,
    .url_read           = file_read,
    .url_write        	= file_write,
    .url_seek         	= file_seek,
    .url_close        	= file_close,
    .url_get_file_handle = file_get_handle,
    .url_check       	= file_check,
};
