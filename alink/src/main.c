#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "linkkit_solo.h"
#include "uart_ecb_task.h"
#include "cJSON.h"

int main(int argc, char **argv)
{
    pthread_t tid;
    printf("main start...\n");

    // pthread_create(&tid, NULL, uart_ecb_task, NULL);

    // linkkit_main("a1YTZpQDGwn", "oE99dmyBcH5RAWE3", "X50_test1", "5fe43d0b7a6b2928c4310cc0d5fcb4b6");
}