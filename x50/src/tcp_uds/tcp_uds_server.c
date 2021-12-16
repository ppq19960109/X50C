#include "main.h"

#include "list.h"
#include "tcp_uds.h"
#include "tcp_uds_server.h"

#define UNIX_DOMAIN "/tmp/unix_server.domain"
static struct Select_Tcp_Server_Event select_Tcp_Server;
static struct Select_Tcp_Server_Event select_Tcp_Client[SELECT_TCP_MAX_CLIENT];

void select_tcp_server_send(char *data, unsigned int len)
{
    for (int i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        send(select_Tcp_Client[i].select_Event.fd, data, len, 0);
    }
}

static int select_tcp_server_recv_cb(void *arg)
{
    struct Select_Server_Event *event = (struct Select_Server_Event *)arg;
    struct Select_Tcp_Server_Event *tcp_event = container_of(event, struct Select_Tcp_Server_Event, select_Event);
    tcp_event->recv_len = recv(event->fd, tcp_event->recv_buf, sizeof(tcp_event->recv_buf), 0); //读取客户端发过来的数据

    if (tcp_event->recv_len <= 0)
    {
        printf("recv[fd=%d] error[%d]:%s\n", event->fd, errno, strerror(errno));

        del_select_server_event(event);
        close(event->fd);
        if (tcp_event->disconnect_cb != NULL)
            tcp_event->disconnect_cb(event->fd);
        event->fd = 0;
        return -1;
    }
    else
    {
        tcp_event->recv_buf[tcp_event->recv_len] = '\0'; //手动添加字符串结束标记
        printf("%s,[%d]: %d\n", __func__, event->fd, tcp_event->recv_len);
        if (tcp_event->recv_cb != NULL)
            tcp_event->recv_cb(tcp_event->recv_buf, tcp_event->recv_len);
    }
    return 0;
}
static int select_tcp_server_except_cb(void *arg)
{
    struct Select_Server_Event *event = (struct Select_Server_Event *)arg;
    // struct Select_Tcp_Server_Event *tcp_event = container_of(event, struct Select_Tcp_Server_Event, select_Event);
    printf("except_cb fd:%d\n", event->fd);
    return 0;
}
static int add_tcp_select_client_event(int cfd)
{
    int i;
    for (i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        if (select_Tcp_Client[i].select_Event.fd == 0)
        {
            select_Tcp_Client[i].select_Event.fd = cfd;
            select_Tcp_Client[i].select_Event.read_cb = select_tcp_server_recv_cb;
            select_Tcp_Client[i].select_Event.except_cb = select_tcp_server_except_cb;
            select_Tcp_Client[i].recv_cb = select_Tcp_Server.recv_cb;
            select_Tcp_Client[i].connect_cb = select_Tcp_Server.connect_cb;
            select_Tcp_Client[i].disconnect_cb = select_Tcp_Server.disconnect_cb;
            if (add_select_server_event(&select_Tcp_Client[i].select_Event) < 0)
            {
                printf("%s:add_select_server_event error\n", __func__);
                select_Tcp_Client[i].select_Event.fd = 0;
                return -1;
            }
            printf("%s:add_select_server_event success\n", __func__);
            if (select_Tcp_Client[i].connect_cb != NULL)
                select_Tcp_Client[i].connect_cb(cfd);
            break;
        }
    }
    if (i == SELECT_MAX_CLIENT)
    {
        return -1;
    }
    return 0;
}

static int select_tcp_server_accetp_cb(void *arg)
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
    res = add_tcp_select_client_event(cfd);
    if (res < 0)
    {
        printf("%s:add_tcp_select_client_event fail\n", __func__);
        close(cfd);
    }
    return res;
}

int (*uds_recv_cb)(char *, unsigned int);
void register_uds_recv_cb(int (*cb)(char *, unsigned int))
{
    uds_recv_cb = cb;
}

int uds_connect(int fd)
{
    printf("uds_connect fd:%d\n", fd);

    return 0;
}

int uds_disconnect(int fd)
{
    printf("uds_disconnect fd:%d\n", fd);
    return 0;
}

void tcp_uds_server_close(void)
{
    printf("tcp_uds_server_close................................\n");
    if (select_Tcp_Server.select_Event.fd != 0)
        close(select_Tcp_Server.select_Event.fd);
    for (int i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        if (select_Tcp_Client[i].select_Event.fd != 0)
            close(select_Tcp_Client[i].select_Event.fd);
    }
    
}
void *tcp_uds_server_task(void *arg)
{
    printf("%s UNIX_DOMAIN:%s\r\n", __func__, UNIX_DOMAIN);
    select_server_init();

    select_Tcp_Server.select_Event.fd = tcp_uds_server_init(NULL, UNIX_DOMAIN, SELECT_TCP_MAX_CLIENT);
    select_Tcp_Server.select_Event.read_cb = select_tcp_server_accetp_cb;
    select_Tcp_Server.select_Event.except_cb = select_tcp_server_except_cb;
    select_Tcp_Server.recv_cb = uds_recv_cb;
    select_Tcp_Server.connect_cb = uds_connect;
    select_Tcp_Server.disconnect_cb = uds_disconnect;
    add_select_server_event(&select_Tcp_Server.select_Event);
    select_server_task(200);

    tcp_uds_server_close();
    return NULL;
}