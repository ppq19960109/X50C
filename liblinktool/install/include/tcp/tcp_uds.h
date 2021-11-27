#ifndef _TCP_UDS_H_
#define _TCP_UDS_H_

int tcp_uds_server_init(int *fd, const char *path, const int listen_num);
int tcp_uds_client_init(int *fd, const char *path, const int delay);

#endif