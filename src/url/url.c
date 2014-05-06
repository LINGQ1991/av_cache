#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "av_string.h"
#include "url.h"

//protrol 域允许字符集
#define URL_SCHEME_CHARS                   		\
    "abcdefghijklmnopqrstuvwxyz"                	\
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"          	\
    "0123456789+-."

// windows本地文件(C:\aaa\bbb)
static inline int is_dos_path(const char *path)
{
#if HAVE_DOS_PATHS
    if (path[0] && path[1] == ':')
        return 1;
#endif
    return 0;
}

//注册URL协议
URLProtocol *first_protocol = NULL;

URLProtocol *av_protocol_next(URLProtocol *p)
{
    if(p) return p->next;
    else  return first_protocol;
}

int ffurl_register_protocol(URLProtocol *protocol, int size)
{
    URLProtocol **p;
    if (size < sizeof(URLProtocol)) {
        URLProtocol *temp = (URLProtocol *)mallocz(sizeof(URLProtocol));
        memcpy(temp, protocol, size);
        protocol = temp;
    }
    p = &first_protocol;
    while (*p != NULL) p = &(*p)->next;
    *p = protocol;
    protocol->next = NULL;
    return 0;
}

//初始化URL

// protocol ://[user:password] @host [:port] / path / [?query][#fragment]
static int url_alloc_for_protocol (URLContext **puc, struct URLProtocol *up,
                                   const char *filename, int flags)
{
    URLContext *uc;
    int err;
/*
#if CONFIG_NETWORK
    if (!ff_network_init())
        return AVERROR(EIO);
#endif
*/
    uc = (URLContext *)mallocz(sizeof(URLContext) + strlen(filename) + 1);
    if (!uc) {
        err = AVERROR(ENOMEM);
        goto fail;
    }
    //uc->av_class = &urlcontext_class;
    uc->filename = (char *) &uc[1]; //uc[1] 指向多申请的内存。&uc[0] = uc; 
    strcpy(uc->filename, filename);
    uc->prot = up;
    uc->flags = flags;
    uc->is_streamed = 0; /* default = not streamed */
    uc->max_packet_size = 0; /* default: stream file */
    if (up->priv_data_size) {
        uc->priv_data = mallocz(up->priv_data_size);
        //if (up->priv_data_class) {
            //*(const AVClass**)uc->priv_data = up->priv_data_class;
            //av_opt_set_defaults(uc->priv_data);
        //}
    }

    *puc = uc;
    return 0;
 fail:
 *puc = NULL;
/*
 #if CONFIG_NETWORK
    ff_network_close();
#endif
*/
    return err;
}

int ffurl_alloc(URLContext **puc, const char *filename, int flags)
{
    URLProtocol *up;
    char proto_str[128];
//	char proto_nested[128], *ptr;
    size_t proto_len = strspn(filename, URL_SCHEME_CHARS); //strspn（返回字符串中第一个不在指定字符串中出现的字符下标）

    if (filename[proto_len] != ':' || is_dos_path(filename))  // file 不是URL协议
        strcpy(proto_str, "file");
    else
        av_strlcpy(proto_str, filename, FFMIN(proto_len+1, sizeof(proto_str)));

	/*
    av_strlcpy(proto_nested, proto_str, sizeof(proto_nested));
    if ((ptr = strchr(proto_nested, '+'))) //strchr(查找字符串s中首次出现字符c的位置)
        *ptr = '\0';								  //为什么把'+' -> '\0'
	*/
	
    up = first_protocol;
    while (up != NULL) {
        if (!strcmp(proto_str, up->name))
            return url_alloc_for_protocol (puc, up, filename, flags);
        //if (up->flags & URL_PROTOCOL_FLAG_NESTED_SCHEME &&
        //    !strcmp(proto_nested, up->name))
        //    return url_alloc_for_protocol (puc, up, filename, flags);
        up = up->next;
    }
    *puc = NULL;
    return AVERROR(ENOENT);
}

//链接
int ffurl_connect(URLContext* uc)
{
    int err = uc->prot->url_open(uc, uc->filename, uc->flags);
    if (err)
        return err;
    uc->is_connected = 1;
    //We must be careful here as ffurl_seek() could be slow, for example for http
    if(   (uc->flags & AVIO_FLAG_WRITE)
       || !strcmp(uc->prot->name, "file")) {
        
		//if(!uc->is_streamed && ffurl_seek(uc, 0, SEEK_SET) < 0)
        //   uc->is_streamed= 1;
	}
    return 0;
}

//打开
int ffurl_open(URLContext **puc, const char *filename, int flags)
{
    int ret = ffurl_alloc(puc, filename, flags);
    if (ret)
        return ret;
    ret = ffurl_connect(*puc);
    if (!ret)
        return 0;
    ffurl_close(*puc);
    *puc = NULL;
    return ret;
}

//关闭
int ffurl_close(URLContext *h)
{
    int ret = 0;
    if (!h) return 0; /* can happen when ffurl_open fails */

    if (h->is_connected && h->prot->url_close)
        ret = h->prot->url_close(h);
//#if CONFIG_NETWORK
//   ff_network_close();
//#endif
    if (h->prot->priv_data_size) {
        free(h->priv_data);
		h->priv_data = NULL;
	}
    free(h);
	h = NULL;
    return ret;
}

static inline int retry_transfer_wrapper(URLContext *h, unsigned char *buf, int size, int size_min,
                                         int (*transfer_func)(URLContext *h, unsigned char *buf, int size))
{
    int ret, len;
    int fast_retries = 5;

    len = 0;
    while (len < size_min) {
        ret = transfer_func(h, buf+len, size-len);
        if (ret == AVERROR(EINTR))
            continue;
        if (h->flags & AVIO_FLAG_NONBLOCK)
            return ret;
        if (ret == AVERROR(EAGAIN)) {
            ret = 0;
            if (fast_retries)
                fast_retries--;
            else
                usleep(1000);
        } else if (ret < 1)
            return ret < 0 ? ret : len;
        if (ret)
           fast_retries = FFMAX(fast_retries, 2);
        len += ret;
        //if (len < size && url_interrupt_cb())
        //    return AVERROR_EXIT;
    }
    return len;
}

int ffurl_read(URLContext *h, unsigned char *buf, int size)
{
    if (!(h->flags & AVIO_FLAG_READ))
        return AVERROR(EIO);
    return retry_transfer_wrapper(h, buf, size, 1, h->prot->url_read);
}

int ffurl_read_complete(URLContext *h, unsigned char *buf, int size)
{
    if (!(h->flags & AVIO_FLAG_READ))
        return AVERROR(EIO);
    return retry_transfer_wrapper(h, buf, size, size, h->prot->url_read);
}

int ffurl_write(URLContext *h, unsigned char *buf, int size)
{
    if (!(h->flags & AVIO_FLAG_WRITE))
        return AVERROR(EIO);
    /* avoid sending too big packets */
    if (h->max_packet_size && size > h->max_packet_size)
        return AVERROR(EIO);

    return retry_transfer_wrapper(h, buf, size, size, (void*)h->prot->url_write);
}

int64_t ffurl_seek(URLContext *h, int64_t pos, int whence)
{
    int64_t ret;

    if (!h->prot->url_seek)
        return AVERROR(ENOSYS);
    ret = h->prot->url_seek(h, pos, whence & ~AVSEEK_FORCE);
    return ret;
}

//分离协议
/* protocol ://[user:password] @host [:port] / path / [?query][#fragment] */
/* rtsp://61.233.202.10/gsp/1.rmvb */
/* http://192.168.1.100:8080/aa/1.rmvb */
/* ftp://192.168.1.100/bb/1.rmvb */
void av_url_split(char *proto, int proto_size,
                  char *authorization, int authorization_size,
                  char *hostname, int hostname_size,
                  int *port_ptr,
                  char *path, int path_size,
                  const char *url)
{
    const char *p, *ls, *at, *col, *brk;

    if (port_ptr)               *port_ptr = -1;
    if (proto_size > 0)         proto[0] = 0;
    if (authorization_size > 0) authorization[0] = 0;
    if (hostname_size > 0)      hostname[0] = 0;
    if (path_size > 0)          path[0] = 0;

    /* parse protocol */
    if ((p = strchr(url, ':'))) {
        av_strlcpy(proto, url, FFMIN(proto_size, p + 1 - url));
        p++; /* skip ':' */
        if (*p == '/') p++;
        if (*p == '/') p++;
    } else {
        /* no protocol means plain filename */
        av_strlcpy(path, url, path_size);
        return;
    }

    /* separate path from hostname */
    ls = strchr(p, '/');
    //if(!ls)
    //    ls = strchr(p, '?');
    if(ls)
        av_strlcpy(path, ls, path_size);
    else
        ls = &p[strlen(p)]; // XXX

    /* the rest is hostname, use that to parse auth/port */
    if (ls != p) {
        /* authorization (user[:pass]@hostname) */
        if ((at = strchr(p, '@')) && at < ls) {
            av_strlcpy(authorization, p,
                       FFMIN(authorization_size, at + 1 - p));
            p = at + 1; /* skip '@' */
        }

        if (*p == '[' && (brk = strchr(p, ']')) && brk < ls) {
            /* [host]:port */
            av_strlcpy(hostname, p + 1,
                       FFMIN(hostname_size, brk - p));
            if (brk[1] == ':' && port_ptr)
                *port_ptr = atoi(brk + 2);
        } else if ((col = strchr(p, ':')) && col < ls) {
            av_strlcpy(hostname, p,
                       FFMIN(col + 1 - p, hostname_size));
            if (port_ptr) *port_ptr = atoi(col + 1);
        } else
            av_strlcpy(hostname, p,
                       FFMIN(ls + 1 - p, hostname_size));
    }
}



