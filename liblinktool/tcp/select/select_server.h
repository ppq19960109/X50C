#ifndef _SELECT_SERVER_H_
#define _SELECT_SERVER_H_

typedef int (*Select_cb)(void *arg);

#define SELECT_MAX_CLIENT 3

struct Select_Server_Event
{
    fd_set rfds, wfds, efds, r_copy_fds;
    struct Select_Client_Event *select_client_event[SELECT_MAX_CLIENT];

    int select_client_num;

    int maxfd;

    int runing;

    Select_cb error_cb;
    Select_cb timeout_cb;
};
struct Select_Client_Event
{
    int fd;    //要监听的文件描述符
    void *arg; //指向自己结构体指针

    Select_cb read_cb;
    Select_cb write_cb;
    Select_cb except_cb;
    Select_cb timeout_cb;
};

void select_server_init(struct Select_Server_Event *server_event);
void select_server_deinit(struct Select_Server_Event *server_event);
void *select_server_task(struct Select_Server_Event *server_event, int time_out);

int add_select_client_event(struct Select_Server_Event *server_event, struct Select_Client_Event *client_event);
void delete_select_client_event(struct Select_Server_Event *server_event, struct Select_Client_Event *client_event);
int ergodic_select_client_timeout(struct Select_Server_Event *server_event);
#endif