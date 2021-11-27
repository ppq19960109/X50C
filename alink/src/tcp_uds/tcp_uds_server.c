#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include "list.h"
#include "tcp_uds.h"
#include "tcp_uds_server.h"

#define UNIX_DOMAIN "/tmp/unix_server.domain"
static struct Select_Tcp_Server_Event select_Tcp_Server;
static struct Select_Tcp_Server_Event select_Tcp_Client[SELECT_TCP_MAX_CLIENT];

static char g_send_buf[1024];
void select_tcp_server_send(char *data, unsigned int len)
{
    pthread_mutex_lock(&select_Tcp_Server.mutex);
    char *send_buf;
    if (len + 8 > sizeof(g_send_buf))
    {
        send_buf = (char *)malloc(len + 8);
    }
    else
    {
        send_buf = g_send_buf;
    }
    send_buf[0] = FRAME_HEADER;
    send_buf[1] = FRAME_HEADER;
    send_buf[2] = len >> 8;
    send_buf[3] = len;
    memcpy(&send_buf[4], data, len);
    send_buf[4 + len] = 0;
    send_buf[5 + len] = 0;
    send_buf[6 + len] = FRAME_TAIL;
    send_buf[7 + len] = FRAME_TAIL;
    int i;
    for (i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        send(select_Tcp_Client[i].select_Event.fd, send_buf, len + 8, 0);
    }

    if (len + 8 > sizeof(g_send_buf))
    {
        free(send_buf);
    }
    pthread_mutex_unlock(&select_Tcp_Server.mutex);
}

static int select_server_recv_cb(void *arg)
{
    struct Select_Server_Event *event = (struct Select_Server_Event *)arg;
    struct Select_Tcp_Server_Event *tcp_event = container_of(event, struct Select_Tcp_Server_Event, select_Event);
    tcp_event->recv_len = recv(event->fd, tcp_event->recv_buf, sizeof(tcp_event->recv_buf), 0); //读取客户端发过来的数据

    if (tcp_event->recv_len <= 0)
    {
        printf("recv[fd=%d] error[%d]:%s\n", event->fd, errno, strerror(errno));

        del_select_server_event(event);
        close(event->fd);
        event->fd = 0;
        if (tcp_event->disconnect_cb != NULL)
            tcp_event->disconnect_cb(event->fd);

        return -1;
    }
    else
    {
        tcp_event->recv_buf[tcp_event->recv_len] = '\0'; //手动添加字符串结束标记
        // printf("socket[%d]: %d,%s\n", fd, tcp_event->recv_len , tcp_event->recv_buf);
        if (tcp_event->recv_cb != NULL)
            tcp_event->recv_cb(tcp_event->recv_buf, tcp_event->recv_len);
    }
    return 0;
}

static int add_select_tcp_client_event(int cfd)
{
    int i;
    for (i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        if (select_Tcp_Client[i].select_Event.fd == 0)
        {
            select_Tcp_Client[i].select_Event.fd = cfd;
            select_Tcp_Client[i].select_Event.read_cb = select_server_recv_cb;
            select_Tcp_Client[i].recv_cb = select_Tcp_Server.recv_cb;
            select_Tcp_Client[i].connect_cb = select_Tcp_Server.connect_cb;
            select_Tcp_Client[i].disconnect_cb = select_Tcp_Server.disconnect_cb;
            if (add_select_server_event(&select_Tcp_Client[i].select_Event) < 0)
            {
                select_Tcp_Client[i].select_Event.fd = 0;
                return -1;
            }
            if (select_Tcp_Client[i].connect_cb != NULL)
                select_Tcp_Client[i].connect_cb(cfd);
        }
    }
    if (i == SELECT_MAX_CLIENT)
    {
        return -1;
    }
    return 0;
}

static int select_server_accetp_cb(void *arg)
{
    struct Select_Server_Event *event = (struct Select_Server_Event *)arg;
    // struct Select_Tcp_Server_Event *tcp_event = container_of(event, struct Select_Tcp_Server_Event, select_Event);
    int res;
    struct sockaddr cin;
    socklen_t clen = sizeof(cin);

    int cfd;
    if ((cfd = accept(event->fd, (struct sockaddr *)&cin, &clen)) == -1)
    {
        if (errno != EAGAIN && errno != EINTR)
        {
            sleep(1);
        }
        printf("%s:accept,%s\n", __func__, strerror(errno));
        return -1;
    }
    printf("%s:accept success\n", __func__);
    res = add_select_tcp_client_event(cfd);
    if (res < 0)
    {
        printf("%s:add_select_tcp_client_event fail\n", __func__);
        close(cfd);
    }
    return res;
}

void select_tcp_server_init(void)
{
    pthread_mutex_init(&select_Tcp_Server.mutex, NULL);
    for (int i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        pthread_mutex_init(&select_Tcp_Client[i].mutex, NULL);
    }
}

void select_tcp_server_deinit(void)
{
    for (int i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        pthread_mutex_destroy(&select_Tcp_Client[i].mutex);
    }
    pthread_mutex_destroy(&select_Tcp_Server.mutex);
}

int (*uds_recv_cb)(char *, unsigned int);

void register_uds_recv_cb(int (*cb)(char *, unsigned int))
{
    uds_recv_cb = cb;
}

int tcp_uds_recv_cb(char *data, unsigned int len)
{
    if (data == NULL)
        return -1;
    int ret = 0;
    int msg_len;
    for (int i = 0; i < len; ++i)
    {
        if (data[i] == FRAME_HEADER && data[i + 1] == FRAME_HEADER)
        {
            msg_len = (data[i + 2] << 8) + data[i + 3];
            if (data[6 + msg_len] != FRAME_TAIL || data[7 + msg_len] != FRAME_TAIL)
            {
                continue;
            }
            printf("tcp_uds_recv_cb msg_len:%d\n", msg_len);
            if (msg_len > 0 && uds_recv_cb != NULL)
            {
                ret = uds_recv_cb(&data[i + 8], msg_len);
                if (ret == 0)
                {
                    i += msg_len + 7;
                }
            }
        }
    }
    return 0;
}

void *tcp_uds_server_task(void *arg)
{
    select_server_init();
    select_tcp_server_init();

    select_Tcp_Server.select_Event.fd = tcp_uds_server_init(NULL, UNIX_DOMAIN, SELECT_TCP_MAX_CLIENT);
    select_Tcp_Server.select_Event.read_cb = select_server_accetp_cb;
    select_Tcp_Server.recv_cb = tcp_uds_recv_cb;
    select_Tcp_Server.connect_cb = NULL;
    select_Tcp_Server.disconnect_cb = NULL;
    add_select_server_event(&select_Tcp_Server.select_Event);
    select_server_task(200);

    select_tcp_server_deinit();
    select_server_deinit();
    return NULL;
}