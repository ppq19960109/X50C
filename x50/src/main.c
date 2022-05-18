#include "main.h"

#include "uart_task.h"
#include "uds_tcp_server.h"
#include "uds_protocol.h"
#include "cloud_platform_task.h"
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
        uart_task_deinit();    // ECB串口资源释放
        uds_protocol_deinit(); // uds相关释放
        cloud_deinit();        //阿里云相关释放
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
    setenv("TZ", "Asia/Shanghai", 1);
    // char val[4] = {6, 2, 3, 4};
    // char val2[4] = {0};
    // int len = 4;
    // printf("H_Kv_Set:%d\n",H_Kv_Set("test",val,4,1));
    // printf("H_Kv_Get:%d\n",H_Kv_Get("test",val2,&len));
    // printf("len:%d value:%d %d %d %d\n",len,val2[0],val2[1],val2[2],val2[3]);
    running = 1;
    int rc = dzlog_init("x50zlog.conf", "default"); // zlog初始化
    if (rc)
    {
        printf("dzlog_init failed\n");
        return -1;
    }
    dzlog_info("main start");

    pthread_t cloud_tid;
    pthread_t uds_tid;

    registerQuitCb(main_quit); //注册退出信号回调
    signalQuit();              //退出信号初始化

    uds_protocol_init(); // uds相关初始化
    cloud_init();        //阿里云相关初始化

    pthread_create(&cloud_tid, NULL, cloud_task, NULL); //阿里云线程启动
    pthread_detach(cloud_tid);

    pthread_create(&uds_tid, NULL, uds_tcp_server_task, NULL); // UI uds通信线程启动
    pthread_detach(uds_tid);

    uart_task(NULL);
    // while (1)
    // {
    //     sleep(1);
    // }
    main_quit();
    // pthread_join(cloud_tid,NULL);
}