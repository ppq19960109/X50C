#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <sys/un.h>

int tcp_uds_server_init(int *fd, const char *path, const int listen_num)
{
    unlink(path);
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, path);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        printf("%s bind error:%s\n", __func__, strerror(errno));
        close(sockfd);
        return -1;
    }
    listen(sockfd, listen_num);

    if (fd != NULL)
    {
        *fd = sockfd;
    }

    return sockfd;
}

int tcp_uds_client_init(int *fd, const char *path, const int delay)
{
    unlink(path);
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    // struct sockaddr_un addr;
    // addr.sun_family = AF_UNIX;
    // strcpy(addr.sun_path, path);

    // if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    // {
    //     printf("%s bind error:%s\n", __func__, strerror(errno));
    //     close(sockfd);
    //     return -1;
    // }

    struct sockaddr_un server_addr;
    server_addr.sun_family = AF_UNIX;
    strcpy(server_addr.sun_path, path);
reconnect:
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        if (delay == 0)
        {
            perror("connect failed:");
            close(sockfd);
            return -1;
        }
        else
        {
            sleep(delay);
            goto reconnect;
        }
    }
    return 0;
}
