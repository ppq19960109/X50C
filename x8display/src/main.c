#include "main.h"

#include "uart_task.h"
#include "uds_tcp_server.h"
#include "uds_protocol.h"
#include "KV_linux.h"
#ifdef DEBUG
#include <mcheck.h>
#endif // DEBUG
static char running = 0;
/*********************************************************************************
 *Function:  main_quit
 *Description： main主函数退出清理资源函数
 *Input:
 *Return:  0:success otherwise:fail
 **********************************************************************************/
static int main_quit(void)
{
    if (running != 0)
    {
        running = 0;
        uds_protocol_deinit(); // uds相关释放
        uart_task_deinit();
        zlog_fini();           // zlog释放
        usleep(1000);
#ifdef DEBUG
        muntrace();
        unsetenv("MALLOC_TRACE");
#endif // DEBUG
    }
    return 0;
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    dzlog_info("debug app main start");
    setenv("MALLOC_TRACE", "./memleak.log", 1);
    mtrace();
#endif // DEBUG
    running = 1;
    int rc = dzlog_init("zlog_display.conf", "default"); // zlog初始化
    if (rc)
    {
        printf("dzlog_init failed\n");
        return -1;
    }
    dzlog_info("main start");
    pthread_t uds_tid;

    registerQuitCb(main_quit); //注册退出信号回调
    signalQuit();              //退出信号初始化

    uart_task_init();
    uds_protocol_init(); // uds相关初始化

    pthread_create(&uds_tid, NULL, uds_tcp_server_task, NULL); // UI uds通信线程启动
    pthread_detach(uds_tid);

    uart_task(NULL);

    main_quit();
}