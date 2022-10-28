#include "tcp.h"
#include "thread2Client.h"

static void *threadHander(void *arg)
{
    int ret;
    ThreadTcp *threadTcp = ((ThreadTcp *)arg);
    threadTcp->status = 1;

    do
    {
        while (threadTcp->status && threadTcp->isServer == 0)
        {
            sleep(2);
            if (tcpClientConnect2(&threadTcp->fd, threadTcp->addr, threadTcp->domain) >= 0)
                break;
        }

        if (threadTcp->connect_cb != NULL)
            threadTcp->connect_cb();

        while (threadTcp->status)
        {
            ret = Recv(threadTcp->fd, threadTcp->recv_buf, sizeof(threadTcp->recv_buf), 0);
            if (ret <= 0)
            {
                printf("thread client Recv ret:%d,error:%d,%s\n", ret, errno, strerror(errno));
                if (threadTcp->disconnect_cb != NULL)
                    threadTcp->disconnect_cb();
                break;
            }
            else
            {
                threadTcp->recv_len = ret;
                threadTcp->recv_buf[threadTcp->recv_len] = '\0';
                if (threadTcp->recv_cb != NULL)
                    threadTcp->recv_cb(threadTcp->recv_buf, threadTcp->recv_len);
            }
        }
        if (threadTcp->fd >= 0)
        {
            Close(threadTcp->fd);
            threadTcp->fd = -1;
        }
    } while (threadTcp->status && threadTcp->isServer == 0);
    threadTcp->status = 0;
    threadTcp->tid = 0;

    pthread_exit(0);
}

int thread2ClientOpen(ThreadTcp *threadTcp)
{
    // pthread_create(&threadTcp->tid, NULL, (void *)threadHander, threadTcp->arg); // clientMethod为此线程客户端，要执行的程序。
    // pthread_detach(threadTcp->tid); //要将id分配出去。

    threadHander(threadTcp->arg);
    return 0;
}

int thread2ClientClose(ThreadTcp *threadTcp)
{
    if (threadTcp->status == 0)
        return -1;
    threadTcp->status = 0;
    usleep(60000);
    if (threadTcp->tid != 0)
    {
        pthread_cancel(threadTcp->tid);
    }
    if (threadTcp->fd >= 0)
    {
        Close(threadTcp->fd);
        threadTcp->fd = -1;
    }
    // sleep(1);

    return 0;
}

int thread2ClientSend(ThreadTcp *threadTcp, void *send, unsigned int len)
{
    if (send == NULL)
        return -1;
    if (threadTcp->fd < 0 || threadTcp->status == 0)
    {
        printf("socketfd is null\n");
        return -1;
    }
    int ret = Send(threadTcp->fd, send, len, 0);

    return ret;
}

void tcpEvent2Set(ThreadTcp *threadTcp, struct sockaddr *addr, int domain, Recv_cb recv_cb, Disconnect_cb disconnect_cb, Connect_cb connect_cb, const int isServer)
{
    threadTcp->fd = -1;
    threadTcp->arg = threadTcp;
    threadTcp->status = 0;
    if (threadTcp->recv_len <= 0)
    {
        memset(threadTcp->recv_buf, 0, sizeof(threadTcp->recv_buf));
        threadTcp->recv_len = 0;
    }
    threadTcp->addr = addr;
    threadTcp->domain = domain;
    threadTcp->recv_cb = recv_cb;
    threadTcp->disconnect_cb = disconnect_cb;
    threadTcp->connect_cb = connect_cb;
    threadTcp->isServer = isServer;
    return;
}