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
#include "tcp.h"
#include "select_tcp_server.h"

static struct Select_Tcp_Server_Event select_Tcp_Server;
static struct Select_Tcp_Server_Event select_Tcp_Client[SELECT_TCP_MAX_CLIENT];

void select_tcp_server_send(char *data, unsigned short len)
{
    pthread_mutex_lock(&select_Tcp_Server.mutex);
    int i;
    for (i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        send(select_Tcp_Client[i].select_Event.fd, data, len, 0);
    }
    pthread_mutex_unlock(&select_Tcp_Server.mutex);
}

static int select_server_recv_cb(void *arg)
{
    struct Select_Server_Event *event=(struct Select_Server_Event *)arg;
    struct Select_Tcp_Server_Event *tcp_event = container_of(event, struct Select_Tcp_Server_Event, select_Event);
    tcp_event->recv_len = recv(event->fd, tcp_event->recv_buf, sizeof(tcp_event->recv_buf), 0); //读取客户端发过来的数据

    if (tcp_event->recv_len <= 0)
    {
        printf("recv[fd=%d] error[%d]:%s\n", event->fd, errno, strerror(errno));

        del_select_server_event(event);
        close(event->fd);
        event->fd = 0;
        if (tcp_event->disconnect_cb != NULL)
            tcp_event->disconnect_cb();

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
                select_Tcp_Client[i].connect_cb();
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
    struct Select_Server_Event *event=(struct Select_Server_Event *)arg;
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

    res = add_select_tcp_client_event(cfd);
    if (res < 0)
    {
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

void select_tcp_server_task()
{
    select_server_init();
    select_tcp_server_init();
    int fd;
    select_Tcp_Server.select_Event.fd = tcpServerListen(&fd, "127.0.0.1", 8888, SELECT_TCP_MAX_CLIENT);
    select_Tcp_Server.select_Event.read_cb = select_server_accetp_cb;
    select_Tcp_Server.recv_cb = NULL;
    select_Tcp_Server.connect_cb = NULL;
    select_Tcp_Server.disconnect_cb = NULL;
    add_select_server_event(&select_Tcp_Server.select_Event);
    select_server_task(200);
    select_tcp_server_deinit();
    select_server_deinit();
}