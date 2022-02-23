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

#include "select_server.h"

void select_server_init(struct Select_Server_Event *server_event)
{
    server_event->runing = 1;
    FD_ZERO(&server_event->rfds);
    FD_ZERO(&server_event->wfds);
    FD_ZERO(&server_event->efds);
    FD_ZERO(&server_event->r_copy_fds);
}

void select_server_deinit(struct Select_Server_Event *server_event)
{
    server_event->runing = 0;
}

int add_select_client_event(struct Select_Server_Event *server_event, struct Select_Client_Event *client_event)
{
    int i;
    for (i = 0; i < SELECT_MAX_CLIENT; ++i)
    {
        if (server_event->select_client_event[i] == NULL)
        {
            server_event->select_client_event[i] = client_event;
            break;
        }
    }
    if (i == SELECT_MAX_CLIENT)
    {
        printf("%s,add full", __func__);
        return -1;
    }

    FD_SET(client_event->fd, &server_event->r_copy_fds);

    if (client_event->fd > server_event->maxfd)
    {
        server_event->maxfd = client_event->fd;
    }
    printf("%s,fd:%d maxfd:%d\n", __func__, client_event->fd, server_event->maxfd);
    printf("%s,select_client_num:%d\n", __func__, ++server_event->select_client_num);
    return 0;
}

void delete_select_client_event(struct Select_Server_Event *server_event, struct Select_Client_Event *client_event)
{
    int i;
    for (i = 0; i < SELECT_MAX_CLIENT; ++i)
    {
        if (server_event->select_client_event[i] != NULL && server_event->select_client_event[i] == client_event)
        {
            printf("%s,fd:%d \n", __func__, client_event->fd);
            if (FD_ISSET(client_event->fd, &server_event->r_copy_fds))
            {
                FD_CLR(client_event->fd, &server_event->r_copy_fds);
            }
            server_event->select_client_event[i] = NULL;
            break;
        }
    }
    printf("%s,select_client_num:%d\n", __func__, --server_event->select_client_num);
}

static void select_client_fd_check(struct Select_Server_Event *server_event, struct Select_Client_Event *client_event)
{
    if (FD_ISSET(client_event->fd, &server_event->rfds))
    {
        if (client_event->read_cb != NULL)
            client_event->read_cb(client_event);
    }
    if (FD_ISSET(client_event->fd, &server_event->wfds))
    {
        if (client_event->write_cb != NULL)
            client_event->write_cb(client_event);
    }
    if (FD_ISSET(client_event->fd, &server_event->efds))
    {
        if (client_event->except_cb != NULL)
            client_event->except_cb(client_event);
    }
}

void *select_server_task(struct Select_Server_Event *server_event, int time_out)
{
    int i;
    int n;
    struct timeval timeout;
    int sec = time_out / 1000;
    int usec = (time_out % 1000) * 1000;
    while (server_event->runing)
    {
        timeout.tv_sec = sec;
        timeout.tv_usec = usec;
        server_event->rfds = server_event->r_copy_fds;
        n = select(server_event->maxfd + 1, &server_event->rfds, NULL, NULL, &timeout);
        if (n < 0)
        {
            perror("select error");
            if (server_event->error_cb != NULL)
                server_event->error_cb((void *)n);
            continue;
        }
        else if (n == 0) // 没有准备就绪的文件描述符  就进入下一次循环
        {
            // printf("select timeout\n");
            if (server_event->timeout_cb != NULL)
                server_event->timeout_cb((void *)n);
            continue;
        }
        else
        {
            // printf("select success\n");
            for (i = 0; i < SELECT_MAX_CLIENT; ++i)
            {
                if (server_event->select_client_event[i] != NULL)
                {
                    select_client_fd_check(server_event, server_event->select_client_event[i]);
                }
            }
        }
    }
    return NULL;
}
