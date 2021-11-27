#ifndef _SELECT_SERVER_H_
#define _SELECT_SERVER_H_

typedef int (*Select_cb)(void *arg);

#define SELECT_MAX_CLIENT 4

struct Select_Server_Event
{
    int fd;    //要监听的文件描述符
    void *arg; //指向自己结构体指针

    Select_cb read_cb;
    Select_cb write_cb;
    Select_cb except_cb;
};

void select_server_init(void);
void select_server_deinit(void);
int add_select_server_event(struct Select_Server_Event *event);
void del_select_server_event(struct Select_Server_Event *event);
void *select_server_task(int time_out);
#endif