#include "main.h"

#include "ecb_uart.h"
#include "cook_assist.h"

#include "uds_protocol.h"
#include "cloud_platform_task.h"
#include "KV_linux.h"
#ifdef DEBUG
#include <mcheck.h>
#endif // DEBUG
hloop_t *g_loop;

void mlog_hex(const void *buf, int len, const char *file, const int line, const char *func)
{
    unsigned char *data = (unsigned char *)buf;
    printf("[%s:%d:%s]", __FILENAME__, __LINE__, __FUNCTION__);
    for (int i = 0; i < len; ++i)
    {
        if (i % 16 == 0)
            printf("\n");
        printf("%02x ", data[i]);
    }
    printf("\n");
}
#define mlogHex(buf, buf_len) mlog_hex(buf, buf_len, __FILENAME__, __LINE__, __FUNCTION__)

static void mlogger(int loglevel, const char *buf, int len)
{
    if (loglevel >= LOG_LEVEL_ERROR)
    {
        stderr_logger(loglevel, buf, len);
        if (loglevel >= LOG_LEVEL_INFO)
        {
            file_logger(loglevel, buf, len);
        }
    }
    else
        stdout_logger(loglevel, buf, len);
    // network_logger(loglevel, buf, len);
}
static void on_reload(void *userdata)
{
    hlogi("reload confile [%s]", g_main_ctx.confile);
}
int main(int argc, char **argv)
{
#ifdef DEBUG
    LOGI("debug app main start");
    setenv("MALLOC_TRACE", "./memleak.log", 1);
    mtrace();
#endif                                           // DEBUG
    int rc = dzlog_init("zlog.conf", "default"); // zlog初始化
    if (rc)
    {
        printf("dzlog_init failed\n");
        return -1;
    }

    signal_init(on_reload, NULL);

    g_loop = hloop_new(0);

    hlog_set_handler(mlogger);
    hlog_set_file("X8GCZ01.log");
    hlog_set_format(DEFAULT_LOG_FORMAT);
    hlog_set_level(LOG_LEVEL_DEBUG);
    hlog_set_remain_days(1);
    logger_enable_color(hlog, 1);
    nlog_listen(g_loop, DEFAULT_LOG_PORT);

    LOGI("main start");
    ecb_uart_init();
    cook_assist_init();
    uds_protocol_init(); // uds相关初始化
    cloud_init();        // 阿里云相关初始化

    hthread_create(cloud_task, NULL);
    hloop_run(g_loop);
    hloop_free(&g_loop);

#ifdef ICE_ENABLE
    display_client_task(NULL);
#else

#endif
}