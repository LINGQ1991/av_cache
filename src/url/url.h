/**
 * @file
 * 
 */

#ifndef URL_H_
#define URL_H_

#define URL_PROTOCOL_FLAG_NESTED_SCHEME 1 /*< The protocol name can be the first part of a nested protocol scheme */

#define AVIO_FLAG_READ  1                                      /**< read-only */
#define AVIO_FLAG_WRITE 2                                      /**< write-only */
#define AVIO_FLAG_READ_WRITE (AVIO_FLAG_READ|AVIO_FLAG_WRITE)  /**< read-write pseudo flag */
#define AVIO_FLAG_NONBLOCK 8

#define AVERROR_EXIT	-1

/**
 * Passing this as the "whence" parameter to a seek function causes it to
 * return the filesize without seeking anywhere. Supporting this is optional.
 * If it is not supported then the seek function will return <0.
 */
#define AVSEEK_SIZE   0x10000

/**
 * Oring this flag as into the "whence" parameter to a seek function causes it to
 * seek by any means (like reopening and linear reading) or other normally unreasonble
 * means that can be extreemly slow.
 * This may be ignored by the seek code.
 */
#define AVSEEK_FORCE 0x20000

/**
 * Oring this flag as into the "whence" parameter to a seek function causes it to
 * seek by any means (like reopening and linear reading) or other normally unreasonble
 * means that can be extreemly slow.
 * This may be ignored by the seek code.
 */
#define AVSEEK_FORCE 0x20000

typedef struct URLContext {
    //const AVClass *av_class;    /**< information for av_log(). Set by url_open(). */
    struct URLProtocol *prot;
    void *priv_data;
    char *filename;             /**< specified URL */
    int flags;
    int max_packet_size;        /**< if non zero, the stream is packetized with this max packet size */
    int is_streamed;            /**< true if streamed (no seek possible), default = false */
    int is_connected;
} URLContext;

typedef struct URLProtocol {
    const char *name;
    int     (*url_open)( URLContext *h, const char *url, int flags);
    int     (*url_read)( URLContext *h, unsigned char *buf, int size);
    int     (*url_write)(URLContext *h, const unsigned char *buf, int size);
    int64_t (*url_seek)( URLContext *h, int64_t pos, int whence);
    int     (*url_close)(URLContext *h);
    struct URLProtocol *next;
    int (*url_read_pause)(URLContext *h, int pause);
    int64_t (*url_read_seek)(URLContext *h, int stream_index,
                             int64_t timestamp, int flags);
    int (*url_get_file_handle)(URLContext *h);
    int priv_data_size;
    //const AVClass *priv_data_class;
    int flags;
    int (*url_check)(URLContext *h, int mask);
} URLProtocol;

/**
 * Register the URLProtocol protocol.
 *
 * @param size the size of the URLProtocol struct referenced
 */
int ffurl_register_protocol(URLProtocol *protocol, int size);

#define REGISTER_PROTOCOL(X,x) { \
    extern URLProtocol ff_##x##_protocol; \
    ffurl_register_protocol(&ff_##x##_protocol, sizeof(ff_##x##_protocol)); }

/**
 * returns the next registered protocol after the given protocol (the first if
 * NULL is given), or NULL if protocol is the last one.
 */
URLProtocol *av_protocol_next(URLProtocol *p);

/**
 * Create a URLContext for accessing to the resource indicated by
 * url, but do not initiate the connection yet.
 *
 * @param puc pointer to the location where, in case of success, the
 * function puts the pointer to the created URLContext
 * @param flags flags which control how the resource indicated by url
 * is to be opened
 * @return 0 in case of success, a negative value corresponding to an
 * AVERROR code in case of failure
 */
int ffurl_alloc(URLContext **h, const char *url, int flags);

/**
 * Connect an URLContext that has been allocated by ffurl_alloc
 */
int ffurl_connect(URLContext *h);

/**
 * Create an URLContext for accessing to the resource indicated by
 * url, and open it.
 *
 * @param puc pointer to the location where, in case of success, the
 * function puts the pointer to the created URLContext
 * @param flags flags which control how the resource indicated by url
 * is to be opened
 * @return 0 in case of success, a negative value corresponding to an
 * AVERROR code in case of failure
 */
int ffurl_open(URLContext **h, const char *url, int flags);

/**
 * Read up to size bytes from the resource accessed by h, and store
 * the read bytes in buf.
 *
 * @return The number of bytes actually read, or a negative value
 * corresponding to an AVERROR code in case of error. A value of zero
 * indicates that it is not possible to read more from the accessed
 * resource (except if the value of the size argument is also zero).
 */
int ffurl_read(URLContext *h, unsigned char *buf, int size);

/**
 * Read as many bytes as possible (up to size), calling the
 * read function multiple times if necessary.
 * This makes special short-read handling in applications
 * unnecessary, if the return value is < size then it is
 * certain there was either an error or the end of file was reached.
 */
int ffurl_read_complete(URLContext *h, unsigned char *buf, int size);

/**
 * Write size bytes from buf to the resource accessed by h.
 *
 * @return the number of bytes actually written, or a negative value
 * corresponding to an AVERROR code in case of failure
 */
int ffurl_write(URLContext *h, unsigned char *buf, int size);

/**
 * Change the position that will be used by the next read/write
 * operation on the resource accessed by h.
 *
 * @param pos specifies the new position to set
 * @param whence specifies how pos should be interpreted, it must be
 * one of SEEK_SET (seek from the beginning), SEEK_CUR (seek from the
 * current position), SEEK_END (seek from the end), or AVSEEK_SIZE
 * (return the filesize of the requested resource, pos is ignored).
 * @return a negative value corresponding to an AVERROR code in case
 * of failure, or the resulting file position, measured in bytes from
 * the beginning of the file. You can use this feature together with
 * SEEK_CUR to read the current file position.
 */
int64_t ffurl_seek(URLContext *h, int64_t pos, int whence);

/**
 * Close the resource accessed by the URLContext h, and free the
 * memory used by it.
 *
 * @return a negative value if an error condition occurred, 0
 * otherwise
 */
int ffurl_close(URLContext *h);

void av_url_split(char *proto, int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname, int hostname_size,
                  int *port_ptr,
                  char *path, int path_size,
                  const char *url);

#endif /* AVFORMAT_URL_H */
