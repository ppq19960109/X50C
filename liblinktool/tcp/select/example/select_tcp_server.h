#ifndef _SELECT_TCP_SERVER_H_
#define _SELECT_TCP_SERVER_H_

#include "select_server.h"

#define SELECT_TCP_MAX_CLIENT 4
#define BUFLEN 4096 /*缓存区大小*/

typedef int (*Recv_cb)(char *data, unsigned int len);
typedef int (*Disconnect_cb)(void);
typedef int (*Connect_cb)(void);

struct Select_Tcp_Server_Event
{
    void *arg; //指向自己结构体指针

    char recv_buf[BUFLEN + 1];
    int recv_len;

    Recv_cb recv_cb;
    Disconnect_cb disconnect_cb;
    Connect_cb connect_cb;

    pthread_mutex_t mutex;
    struct Select_Server_Event select_Event;
};
void select_tcp_server_send(char *data, unsigned short len);
#endif