#ifndef _THREAD2CLIENT_H_
#define _THREAD2CLIENT_H_

#include <pthread.h>
#define BUF_LEN (4 * 1024)

typedef int (*Recv_cb)(char *data, unsigned int len);
typedef int (*Connect_cb)(void);
typedef int (*Disconnect_cb)(void);

typedef struct
{
    int status;

    char recv_buf[BUF_LEN + 1];
    int recv_len;

    int fd;
    pthread_t tid;
    void *arg;

    struct sockaddr *addr;
    int domain;
    Recv_cb recv_cb;             //回调函数
    Disconnect_cb disconnect_cb; //回调函数
    Connect_cb connect_cb;
    int isServer;
} ThreadTcp;

int thread2ClientOpen(ThreadTcp *threadTcp);
int thread2ClientClose(ThreadTcp *threadTcp);
int thread2ClientSend(ThreadTcp *threadTcp, void *send, unsigned int len);
void tcpEvent2Set(ThreadTcp *threadTcp, struct sockaddr *addr, int domain, Recv_cb recv_cb, Disconnect_cb disconnect_cb, Connect_cb connect_cb, const int isServer);
#endif