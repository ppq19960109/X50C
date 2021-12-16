#include "main.h"

#include "uart_ecb_task.h"
#include "tcp_uds_server.h"
#include "uds_protocol.h"
#include "uart_cloud_task.h"
#include "uart_gesture_task.h"

static int main_quit(void)
{
    uds_protocol_deinit();
    cloud_deinit();
    zlog_fini();
    return 0;
}
int main(int argc, char **argv)
{
    int rc = dzlog_init("x50_log.conf", "default");
    if (rc)
    {
        printf("dzlog_init failed\n");
        return -1;
    }
    dzlog_info("main start");

    pthread_t uart_tid;
    pthread_t uart_gesture_tid;
    pthread_t uds_tid;
    
    registerQuitCb(main_quit);
    signalQuit();

    cloud_init();
    uds_protocol_init();
    pthread_create(&uds_tid, NULL, tcp_uds_server_task, NULL);
    pthread_create(&uart_tid, NULL, uart_ecb_task, NULL);
    // pthread_create(&uart_gesture_tid, NULL, uart_gesture_task, NULL);
    cloud_task(NULL);
    // while (1)
    // {
    //     sleep(2);
    // }
    main_quit();
}