#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
//#include "internal.h"
//#include "network.h"
#include "url.h"
#include "common.h"
#include "av_string.h"
#include "tcp.h"


static inline int ff_network_wait_fd(int fd, int write)
{
    int ev = write ? POLLOUT : POLLIN;
    struct pollfd p = { .fd = fd, .events = ev, .revents = 0 };
    int ret;
    ret = poll(&p, 1, 100);
    return ret < 0 ? ff_neterrno() : p.revents & (ev | POLLERR | POLLHUP) ? 0 : AVERROR(EAGAIN);
}

int ff_socket_nonblock(int socket, int enable)
{
   if (enable)
      return fcntl(socket, F_SETFL, fcntl(socket, F_GETFL) | O_NONBLOCK);
   else
      return fcntl(socket, F_SETFL, fcntl(socket, F_GETFL) & ~O_NONBLOCK);
}

void ff_freeaddrinfo(struct addrinfo *res)
{
    free(res->ai_canonname);
    free(res->ai_addr);
    free(res);
}

int av_find_info_tag(char *arg, int arg_size, const char *tag1, const char *info)
{
    const char *p;
    char tag[128], *q;

    p = info;
    if (*p == '?')
        p++;
    for(;;) {
        q = tag;
        while (*p != '\0' && *p != '=' && *p != '&') {
            if ((q - tag) < sizeof(tag) - 1)
                *q++ = *p;
            p++;
        }
        *q = '\0';
        q = arg;
        if (*p == '=') {
            p++;
            while (*p != '&' && *p != '\0') {
                if ((q - arg) < arg_size - 1) {
                    if (*p == '+')
                        *q++ = ' ';
                    else
                        *q++ = *p;
                }
                p++;
            }
        }
        *q = '\0';
        if (!strcmp(tag, tag1))
            return 1;
        if (*p != '&')
            break;
        p++;
    }
    return 0;
}


/* return non zero if error */
static int tcp_open(URLContext *h, const char *uri, int flags)
{
	struct addrinfo hints, *ai, *cur_ai;
    int port, fd = -1;
    TCPContext *s = NULL;
    int listen_socket = 0;
    const char *p;
    char buf[256];
    int ret;
    socklen_t optlen;
    int timeout = 50;
    char hostname[1024],proto[1024],path[1024];
    char portstr[10];

	//解析URL协议
    av_url_split(proto, sizeof(proto), NULL, 0, hostname, sizeof(hostname),
        &port, path, sizeof(path), uri);
    if (strcmp(proto,"tcp") || port <= 0 || port >= 65536)
        return AVERROR(EINVAL);

	p = strchr(uri, '?');
    if (p) {
        if (av_find_info_tag(buf, sizeof(buf), "listen", p))
            listen_socket = 1;
        if (av_find_info_tag(buf, sizeof(buf), "timeout", p)) {
            timeout = strtol(buf, NULL, 10);
        }
    }
	memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port);
    ret = getaddrinfo(hostname, portstr, &hints, &ai);
    if (ret) {
        //av_log(h, AV_LOG_ERROR,
              // "Failed to resolve hostname %s: %s\n",
               //hostname, gai_strerror(ret));
        return AVERROR(EIO);
    }
	
	cur_ai = ai;
	
restart:
    ret = AVERROR(EIO);
    fd = socket(cur_ai->ai_family, cur_ai->ai_socktype, cur_ai->ai_protocol);
    if (fd < 0) {
        printf("failed fd:%d\n", fd);
		goto fail;
	}
	printf("sucess fd:%d\n", fd);
	
    if (listen_socket) {
        int fd1;
        ret = bind(fd, cur_ai->ai_addr, cur_ai->ai_addrlen);
        listen(fd, 1);
        fd1 = accept(fd, NULL, NULL);
        closesocket(fd);
        fd = fd1;
        //ff_socket_nonblock(fd, 1);
    } else {
 redo:
        //ff_socket_nonblock(fd, 1);
        ret = connect(fd, cur_ai->ai_addr, cur_ai->ai_addrlen);
		printf("connect ret :%d\n", ret);
    }

    if (ret < 0) {
        struct pollfd p = {fd, POLLOUT, 0};
        ret = AVERROR(errno);
        if (ret == AVERROR(EINTR)) {
            //if (url_interrupt_cb()) {
            //    ret = AVERROR_EXIT;
            //   goto fail1;
            //}
            goto redo;
        }
        if (ret != AVERROR(EINPROGRESS) &&
            ret != AVERROR(EAGAIN))
            goto fail;

        /* wait until we are connected or until abort */
        while(timeout--) {
            //if (url_interrupt_cb()) {
            //    ret = AVERROR_EXIT;
            //    goto fail1;
            //}
            ret = poll(&p, 1, 100);
            if (ret > 0)
                break;
        }
        if (ret <= 0) {
            ret = AVERROR(ETIMEDOUT);
            goto fail;
        }
        /* test error */
        optlen = sizeof(ret);
        getsockopt (fd, SOL_SOCKET, SO_ERROR, &ret, &optlen);
        if (ret != 0) {
            //av_log(h, AV_LOG_ERROR,
            //       "TCP connection to %s:%d failed: %s\n",
            //       hostname, port, strerror(ret));
            ret = AVERROR(ret);
            goto fail;
        }
    }
    s = mallocz(sizeof(TCPContext));
    if (!s) {
        freeaddrinfo(ai);
        return AVERROR(ENOMEM);
    }
    h->priv_data = s;
    h->is_streamed = 1;
    s->fd = fd;
    freeaddrinfo(ai);
    return 0;

 fail:
    if (cur_ai->ai_next) {
        /* Retry with the next sockaddr */
        cur_ai = cur_ai->ai_next;
        if (fd >= 0)
            closesocket(fd);
        goto restart;
    }
 //fail1:
 //   if (fd >= 0)
 //       closesocket(fd);
 //   freeaddrinfo(ai);
 //   return ret;
}

static int tcp_read(URLContext *h, uint8_t *buf, int size)
{
	TCPContext *s = h->priv_data;
    int ret;

	/*
    if (!(h->flags & AVIO_FLAG_NONBLOCK)) {
        ret = ff_network_wait_fd(s->fd, 0);
        if (ret < 0)
            return ret;
    }
	*/
    ret = recv(s->fd, buf, size, 0);
    return ret < 0 ? ff_neterrno() : ret;
	return 0;
}

static int tcp_write(URLContext *h, const uint8_t *buf, int size)
{
    TCPContext *s = h->priv_data;
    int ret;

	/*
    if (!(h->flags & AVIO_FLAG_NONBLOCK)) {
        ret = ff_network_wait_fd(s->fd, 1);
        if (ret < 0)
            return ret;
    }
	*/
    ret = send(s->fd, buf, size, 0);
    return ret < 0 ? ff_neterrno() : ret;
	return 0;
}

static int tcp_close(URLContext *h)
{
    TCPContext *s = h->priv_data;
    closesocket(s->fd);
    free(s);
    return 0;
}

static int tcp_get_file_handle(URLContext *h)
{
	TCPContext *s = h->priv_data;
    return s->fd;
}

URLProtocol ff_tcp_protocol = {
    .name					= "tcp",
    .url_open 				= tcp_open,
    .url_read            	= tcp_read,
    .url_write           	= tcp_write,
    .url_close           	= tcp_close,
    .url_get_file_handle = tcp_get_file_handle,
};
