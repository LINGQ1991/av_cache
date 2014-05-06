/**
 * @file
 * 
 */

#ifndef TCP_H_
#define TCP_H_

typedef struct TCPContext {
    int fd;
} TCPContext;

#define closesocket close

/*
#if !HAVE_STRUCT_ADDRINFO
struct addrinfo {
    int ai_flags;   //ָ��������������ַ������
    int ai_family;  //��ַ�� AF_INET:IPv4  AF_INET6:IPv6  AF_UNSPEC:Э���޹�
    int ai_socktype;//socket���� SOCK_STREAM:��   SOCK_DGRAM:���ݱ�
    int ai_protocol;//Э������  ȡ��ֵȡ����ai_address��ai_socktype��ֵ
    int ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
#endif
*/
#if !HAVE_POLL_H
typedef unsigned long nfds_t;

struct pollfd {
    int fd;
    short events;  /* events to look for */
    short revents; /* events that occurred */
};

/* events & revents */
#define POLLIN     0x0001  /* any readable data available */
#define POLLOUT    0x0002  /* file descriptor is writeable */
#define POLLRDNORM POLLIN
#define POLLWRNORM POLLOUT
#define POLLRDBAND 0x0008  /* priority readable data */
#define POLLWRBAND 0x0010  /* priority data can be written */
#define POLLPRI    0x0020  /* high priority readable data */

/* revents only */
#define POLLERR    0x0004  /* errors pending */
#define POLLHUP    0x0080  /* disconnected */
#define POLLNVAL   0x1000  /* invalid file descriptor */

int poll(struct pollfd *fds, nfds_t numfds, int timeout);
#endif /* HAVE_POLL_H */

#define ff_neterrno() AVERROR(errno)


#endif //TCP_H_