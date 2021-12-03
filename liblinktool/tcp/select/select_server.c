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

static struct Select_Server_Event *select_server_event[SELECT_MAX_CLIENT] = {0};
static fd_set rfds, wfds, efds, r_copy_fds;
static int maxfd = 0;
static int select_client_num = 0;
static int runing = 0;
void select_server_init(void)
{
    runing=1;
    FD_ZERO(&rfds);
    FD_ZERO(&r_copy_fds);
}

void select_server_deinit(void)
{
    runing=0;
}

int add_select_server_event(struct Select_Server_Event *event)
{
    int i;
    for (i = 0; i < SELECT_MAX_CLIENT; ++i)
    {
        if (select_server_event[i] == NULL)
        {
            select_server_event[i] = event;
            break;
        }
    }
    if (i == SELECT_MAX_CLIENT)
    {
        printf("%s,add full", __func__);
        return -1;
    }

    FD_SET(event->fd, &r_copy_fds);

    if (event->fd > maxfd)
    {
        maxfd = event->fd;
    }
    printf("%s,fd:%d maxfd:%d\n", __func__, event->fd, maxfd);
    printf("%s,select_client_num:%d\n", __func__, ++select_client_num);
    return 0;
}

void del_select_server_event(struct Select_Server_Event *event)
{
    int i;
    for (i = 0; i < SELECT_MAX_CLIENT; ++i)
    {
        if (select_server_event[i] != NULL && select_server_event[i] == event)
        {
            printf("%s,fd:%d \n", __func__, event->fd);
            if (FD_ISSET(event->fd, &r_copy_fds))
            {
                FD_CLR(event->fd, &r_copy_fds);
            }
            select_server_event[i] = NULL;
            break;
        }
    }
    printf("%s,select_client_num:%d\n", __func__, --select_client_num);
}

static void select_fd_check(struct Select_Server_Event *event)
{
    if (FD_ISSET(event->fd, &rfds))
    {
        if (event->read_cb != NULL)
            event->read_cb(event);
    }
    if (FD_ISSET(event->fd, &wfds))
    {
        if (event->write_cb != NULL)
            event->write_cb(event);
    }
    if (FD_ISSET(event->fd, &efds))
    {
        if (event->except_cb != NULL)
            event->except_cb(event);
    }
}

void *select_server_task(int time_out)
{
    int i;
    int n;
    struct timeval timeout;
    timeout.tv_sec = time_out / 1000;
    timeout.tv_usec = (time_out % 1000) * 1000;

    while (runing)
    {
        rfds = r_copy_fds;
        n = select(maxfd + 1, &rfds, NULL, NULL, &timeout);
        if (n < 0)
        {
            perror("select error");
            continue;
        }
        else if (n == 0) // 没有准备就绪的文件描述符  就进入下一次循环
        {
            // printf("select timeout\n");
            continue;
        }
        else
        {
            printf("select success\n");
            for (i = 0; i < SELECT_MAX_CLIENT; ++i)
            {
                if (select_server_event[i] != NULL)
                {
                    select_fd_check(select_server_event[i]);
                }
            }
        }
    }
}
