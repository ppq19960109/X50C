#include "main.h"

#include "uart_ecb_task.h"
#include "tcp_uds_server.h"
#include "uds_protocol.h"
#include "uart_cloud_task.h"
#include "uart_gesture_task.h"

static int running = 0;

static int main_quit(void)
{
    if (running != 0)
    {
        running = 0;
        uart_ecb_task_close();
        uart_gesture_task_close();
        uds_protocol_deinit(); //uds相关释放
        cloud_deinit();        //阿里云相关释放
        zlog_fini();           //zlog释放
        usleep(1000);
    }
    return 0;
}
int main(int argc, char **argv)
{
    running = 1;
    int rc = dzlog_init("x50_log.conf", "default"); //zlog初始化
    if (rc)
    {
        printf("dzlog_init failed\n");
        return -1;
    }
    dzlog_info("main start");

    pthread_t uart_tid;
    pthread_t uart_gesture_tid;
    pthread_t uds_tid;

    registerQuitCb(main_quit); //注册退出信号回调
    signalQuit();              //退出信号初始化

    uds_protocol_init(); //uds相关初始化
    cloud_init();        //阿里云相关初始化

    pthread_create(&uart_tid, NULL, uart_ecb_task, NULL); //电控板线程启动
    pthread_detach(uart_tid);
    pthread_create(&uart_gesture_tid, NULL, uart_gesture_task, NULL); //手势线程启动
    pthread_detach(uart_gesture_tid);
    // sleep(1);
    pthread_create(&uds_tid, NULL, tcp_uds_server_task, NULL); //UI uds通信线程启动
    pthread_detach(uds_tid);
    cloud_task(NULL); //阿里云线程启动
    // while (1)
    // {
    //     sleep(2);
    // }
    main_quit();
    // pthread_join(uart_tid,NULL);
}