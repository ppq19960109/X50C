#include "main.h"

#include "list.h"
#include "tcp_uds.h"
#include "uds_tcp_server.h"

#define UNIX_DOMAIN "/tmp/unix_server.domain"
static struct Select_Server_Event select_server_event;
static struct App_Select_Client_Tcp app_select_client_Tcp_Server;
static struct App_Select_Client_Tcp app_select_client_Tcp_Client[SELECT_TCP_MAX_CLIENT];

void app_select_client_tcp_server_send(char *data, unsigned int len)
{
    for (int i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        send(app_select_client_Tcp_Client[i].select_client_event.fd, data, len, 0);
    }
}

static int app_select_tcp_server_recv_cb(void *arg)
{
    struct Select_Client_Event *client_event = (struct Select_Client_Event *)arg;
    struct App_Select_Client_Tcp *app_select_tcp = container_of(client_event, struct App_Select_Client_Tcp, select_client_event);
    app_select_tcp->recv_len = recv(client_event->fd, app_select_tcp->recv_buf, sizeof(app_select_tcp->recv_buf), 0); //读取客户端发过来的数据

    if (app_select_tcp->recv_len <= 0)
    {
        printf("recv[fd=%d] error[%d]:%s\n", client_event->fd, errno, strerror(errno));

        delete_select_client_event(&select_server_event, client_event);
        close(client_event->fd);
        if (app_select_tcp->disconnect_cb != NULL)
            app_select_tcp->disconnect_cb(client_event->fd);
        client_event->fd = 0;
        return -1;
    }
    else
    {
        app_select_tcp->recv_buf[app_select_tcp->recv_len] = '\0'; //手动添加字符串结束标记
        printf("%s,[%d]: %d\n", __func__, client_event->fd, app_select_tcp->recv_len);
        if (app_select_tcp->recv_cb != NULL)
            app_select_tcp->recv_cb(app_select_tcp->recv_buf, app_select_tcp->recv_len);
    }
    return 0;
}
static int app_select_tcp_server_except_cb(void *arg)
{
    struct Select_Client_Event *client_event = (struct Select_Client_Event *)arg;
    // struct App_Select_Client_Tcp *app_select_tcp = container_of(client_event, struct App_Select_Client_Tcp, select_client_event);
    printf("except_cb fd:%d\n", client_event->fd);
    return 0;
}
static int app_add_tcp_select_client_event(int cfd)
{
    int i;
    for (i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        if (app_select_client_Tcp_Client[i].select_client_event.fd == 0)
        {
            app_select_client_Tcp_Client[i].select_client_event.fd = cfd;
            app_select_client_Tcp_Client[i].select_client_event.read_cb = app_select_tcp_server_recv_cb;
            app_select_client_Tcp_Client[i].select_client_event.except_cb = app_select_tcp_server_except_cb;
            app_select_client_Tcp_Client[i].recv_cb = app_select_client_Tcp_Server.recv_cb;
            app_select_client_Tcp_Client[i].connect_cb = app_select_client_Tcp_Server.connect_cb;
            app_select_client_Tcp_Client[i].disconnect_cb = app_select_client_Tcp_Server.disconnect_cb;
            if (add_select_client_event(&select_server_event, &app_select_client_Tcp_Client[i].select_client_event) < 0)
            {
                printf("%s:add_select_client_event error\n", __func__);
                app_select_client_Tcp_Client[i].select_client_event.fd = 0;
                return -1;
            }
            printf("%s:add_select_client_event success\n", __func__);
            if (app_select_client_Tcp_Client[i].connect_cb != NULL)
                app_select_client_Tcp_Client[i].connect_cb(cfd);
            break;
        }
    }
    if (i == SELECT_MAX_CLIENT)
    {
        return -1;
    }
    return 0;
}

static int app_select_tcp_server_accetp_cb(void *arg) //uds服务端accetp回调
{
    struct Select_Client_Event *client_event = (struct Select_Client_Event *)arg;
    // struct App_Select_Client_Tcp *app_select_tcp = container_of(client_event, struct App_Select_Client_Tcp, select_client_event);
    int res;
    struct sockaddr cin;
    socklen_t clen = sizeof(cin);

    int cfd;
    if ((cfd = accept(client_event->fd, (struct sockaddr *)&cin, &clen)) == -1)
    {
        if (errno != EAGAIN && errno != EINTR)
        {
            sleep(1);
        }
        printf("%s:accept,%s\n", __func__, strerror(errno));
        return -1;
    }
    printf("%s:accept success\n", __func__);
    res = app_add_tcp_select_client_event(cfd);
    if (res < 0)
    {
        printf("%s:app_add_tcp_select_client_event fail\n", __func__);
        close(cfd);
    }
    return res;
}

int (*uds_recv_cb)(char *, unsigned int);
void register_uds_recv_cb(int (*cb)(char *, unsigned int)) //uds接受数据回调
{
    uds_recv_cb = cb;
}

int uds_connect(int fd) //uds连接回调
{
    printf("uds_connect fd:%d\n", fd);
    return 0;
}

int uds_disconnect(int fd) //uds断开连接回调
{
    printf("uds_disconnect fd:%d\n", fd);
    return 0;
}

void uds_tcp_server_close(void)
{
    printf("tcp_uds_server_close...\n");
    if (app_select_client_Tcp_Server.select_client_event.fd != 0)
        close(app_select_client_Tcp_Server.select_client_event.fd);
    for (int i = 0; i < SELECT_TCP_MAX_CLIENT; ++i)
    {
        if (app_select_client_Tcp_Client[i].select_client_event.fd != 0)
            close(app_select_client_Tcp_Client[i].select_client_event.fd);
    }
}
void *uds_tcp_server_task(void *arg) //uds任务
{
    printf("%s UNIX_DOMAIN:%s\r\n", __func__, UNIX_DOMAIN);
    select_server_init(&select_server_event);

    app_select_client_Tcp_Server.select_client_event.fd = tcp_uds_server_init(NULL, UNIX_DOMAIN, SELECT_TCP_MAX_CLIENT); //uds server fd初始化
    app_select_client_Tcp_Server.select_client_event.read_cb = app_select_tcp_server_accetp_cb;
    app_select_client_Tcp_Server.select_client_event.except_cb = app_select_tcp_server_except_cb;
    app_select_client_Tcp_Server.recv_cb = uds_recv_cb;
    app_select_client_Tcp_Server.connect_cb = uds_connect;
    app_select_client_Tcp_Server.disconnect_cb = uds_disconnect;
    add_select_client_event(&select_server_event, &app_select_client_Tcp_Server.select_client_event); //添加uds server fd进select
    select_server_task(&select_server_event, 400);

    uds_tcp_server_close();
    return NULL;
}
void uds_tcp_server_task_deinit(void)
{
    select_server_deinit(&select_server_event);
}