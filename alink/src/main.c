#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "signalQuit.h"

#include "uart_ecb_task.h"
#include "tcp_uds_server.h"
#include "uds_protocol.h"
#include "cloud_task.h"

static int main_quit(void)
{
    uds_protocol_deinit();
    cloud_deinit();
}
int main(int argc, char **argv)
{
    pthread_t uart_tid;
    pthread_t uds_tid;
    printf("main start...\n");
    registerQuitCb(main_quit);
    signalQuit();

    cloud_init();
    uds_protocol_init();
    pthread_create(&uds_tid, NULL, tcp_uds_server_task, NULL);
    pthread_create(&uart_tid, NULL, uart_ecb_task, NULL);
    // cloud_task(NULL);
    while (1)
    {
        sleep(2);
    }
    main_quit();
}