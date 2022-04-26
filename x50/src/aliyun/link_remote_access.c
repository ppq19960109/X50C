
#include "link_remote_access.h"
#ifdef REMOTE_ACCESS
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"
#include "aiot_ra_api.h"

static void *ra_handle = NULL;
static pthread_t g_ra_process_thread;

aiot_ra_service_t services[] = {
    {
        .type = "_SSH",
        .ip = "127.0.0.1",
        .port = 22,
    },
};

void ra_event_cb(void *handle, const aiot_ra_event_t *event, void *userdata)
{
    switch (event->type)
    {
    case AIOT_RA_EVT_CONNECT:
        printf("ra_event_cb AIOT_RA_EVT_CONNECT %s \r\n", event->tunnel_id);
        /* TODO: 告知websocket建连成功, 不可以在这里调用耗时较长的阻塞函数 */
        break;
    case AIOT_RA_EVT_DISCONNECT:
        printf("ra_event_cb AIOT_RA_EVT_DISCONNECT %s \r\n", event->tunnel_id);
        /* TODO: 告知websocket掉线, 不可以在这里调用耗时较长的阻塞函数 */
        break;
    case AIOT_RA_EVT_OPEN_WEBSOCKET:
        printf("ra_event_cb AIOT_RA_EVT_OPEN_WEBSOCKET %s \r\n", event->tunnel_id);
        /* TODO: 告知RA接收到打开websocket链接命令, 不可以在这里调用耗时较长的阻塞函数 */
        break;
    case AIOT_RA_EVT_CLOSE_WEBSOCKET:
        printf("ra_event_cb AIOT_RA_EVT_CLOSE_WEBSOCKET %s \r\n", event->tunnel_id);
        /* TODO: 告知RA接收到关闭websocket链接命令, 不可以在这里调用耗时较长的阻塞函数 */
        break;
    }
}

int link_remote_access_open(void *mqtt_handle, void *cred)
{
    /* 创建1个RA实例并内部初始化默认参数 */
    ra_handle = aiot_ra_init();
    if (ra_handle == NULL)
    {
        printf("aiot_ra_init failed\n");
        return -1;
    }

    /* 配置MQTT句柄，ra内部会订阅MQTT的消息 */
    aiot_ra_setopt(ra_handle, AIOT_RAOPT_MQTT_HANDLE, mqtt_handle);
    /* 配置网络连接的安全凭据, 上面已经创建好了 */
    aiot_ra_setopt(ra_handle, AIOT_RAOPT_NETWORK_CRED, (void *)cred);
    /* 配置RA内部事件回调函数， 可选*/
    aiot_ra_setopt(ra_handle, AIOT_RAOPT_EVENT_HANDLER, (void *)ra_event_cb);
    /* 配置本地可支持的服务 */
    for (int i = 0; i < sizeof(services) / sizeof(aiot_ra_service_t); i++)
    {
        aiot_ra_setopt(ra_handle, AIOT_RAOPT_ADD_SERVICE, (void *)&services[i]);
    }
    /*开启线程，运行RA服务*/
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (0 != pthread_create(&g_ra_process_thread, &attr, aiot_ra_start, (void *)ra_handle))
    {
        printf("creat remote_proxy_thread error!");
        return -1;
    }

    /* 设备主动请求隧道建连，该隧道只用于远程登录 */
    aiot_ra_request(ra_handle);
    return 0;
}

int link_remote_access_close(void)
{
    aiot_ra_stop(ra_handle);
    /* 主循环进入休眠 */
    void *result = NULL;
    pthread_join(g_ra_process_thread, &result);
    if (NULL != result)
    {
        /* 打印出线程退出的状态码 */
        printf("pthread exit state -0x%04X\n", *(int32_t *)result * -1);
    }

    aiot_ra_deinit(&ra_handle);

    sleep(1);

    return 0;
}
#endif