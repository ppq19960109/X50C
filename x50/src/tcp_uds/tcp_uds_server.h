#ifndef _TCP_UDS_SERVER_H_
#define _TCP_UDS_SERVER_H_

#include "select_server.h"

#define SELECT_TCP_MAX_CLIENT 2
#define BUFLEN 4096 /*缓存区大小*/

typedef int (*Recv_cb)(char *data, unsigned int len);
typedef int (*Disconnect_cb)(int fd);
typedef int (*Connect_cb)(int fd);

struct Select_Tcp_Server_Event
{
    void *arg; //指向自己结构体指针

    char recv_buf[BUFLEN + 1];
    int recv_len;

    Recv_cb recv_cb;
    Disconnect_cb disconnect_cb;
    Connect_cb connect_cb;

    struct Select_Server_Event select_Event;
};

void select_tcp_server_send(char *data, unsigned int len);
void register_uds_recv_cb(int (*cb)(char *, unsigned int));
void *tcp_uds_server_task(void *arg);

#endif