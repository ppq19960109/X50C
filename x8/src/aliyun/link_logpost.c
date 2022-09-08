#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"
#include "aiot_logpost_api.h"

static void *logpost_handle = NULL;

/* 事件处理回调, 用户可通过此回调获取日志上报通道的开关状态 */
static void demo_logpost_event_handler(void *handle, const aiot_logpost_event_t *event, void *userdata)
{
    switch (event->type)
    {
    /* 日志配置事件, 当设备连云成功或者用户在控制台页面控制日志开关时会收到此事件 */
    case AIOT_LOGPOSTEVT_CONFIG_DATA:
    {
        printf("user log switch state is: %d\r\n", event->data.config_data.on_off);
        printf("toggle it using the switch in device detail page in https://iot.console.aliyun.com\r\n");
    }
    default:
        break;
    }
}

/* 上报日志到云端 */
void link_send_log(char *log)
{
    if (logpost_handle == NULL)
        return;
    int32_t res = 0;
    aiot_logpost_msg_t msg;

    memset(&msg, 0, sizeof(aiot_logpost_msg_t));
    msg.timestamp = 0;                       /* 单位为ms的时间戳, 填写0则SDK将使用当前的时间戳 */
    msg.loglevel = AIOT_LOGPOST_LEVEL_DEBUG; /* 日志级别 */
    msg.module_name = "APP";                 /* 日志对应的模块 */
    msg.code = 200;                          /* 状态码 */
    msg.msg_id = 0;                          /* 云端下行报文的消息标示符, 若无对应消息可直接填0 */
    msg.content = log;                       /* 日志内容 */

    res = aiot_logpost_send(logpost_handle, &msg);
    if (res < 0)
    {
        printf("aiot_logpost_send failed: -0x%04X\r\n", -res);
    }
}

int link_logopost_init(void *mqtt_handle)
{
    int32_t res = STATE_SUCCESS;
    uint8_t sys_log_switch = 1;

    /* 创建1个logpost客户端实例并内部初始化默认参数 */
    logpost_handle = aiot_logpost_init();
    if (logpost_handle == NULL)
    {
        printf("aiot_logpost_init failed\r\n");
        return -1;
    }

    /* 配置logpost的系统日志开关, 打开后将上报网络延时信息 */
    res = aiot_logpost_setopt(logpost_handle, AIOT_LOGPOSTOPT_SYS_LOG, (void *)&sys_log_switch);
    if (res < STATE_SUCCESS)
    {
        printf("aiot_logpost_setopt AIOT_LOGPOSTOPT_SYS_LOG failed, res: -0x%04X\r\n", -res);
        aiot_logpost_deinit(&logpost_handle);
        return -1;
    }

    /* 配置logpost会话, 把它和MQTT会话的句柄关联起来 */
    res = aiot_logpost_setopt(logpost_handle, AIOT_LOGPOSTOPT_MQTT_HANDLE, mqtt_handle);
    if (res < STATE_SUCCESS)
    {
        printf("aiot_logpost_setopt AIOT_LOGPOSTOPT_MQTT_HANDLE failed, res: -0x%04X\r\n", -res);
        aiot_logpost_deinit(&logpost_handle);
        return -1;
    }

    res = aiot_logpost_setopt(logpost_handle, AIOT_LOGPOSTOPT_EVENT_HANDLER, (void *)demo_logpost_event_handler);
    if (res < STATE_SUCCESS)
    {
        printf("aiot_logpost_setopt AIOT_LOGPOSTOPT_EVENT_HANDLER failed, res: -0x%04X\r\n", -res);
        aiot_logpost_deinit(&logpost_handle);
        return -1;
    }
    return 0;
}

int link_logopost_deinit()
{
    if (logpost_handle == NULL)
        return -1;
    int32_t res = STATE_SUCCESS;
    /* 销毁logpost实例, 一般不会运行到这里 */
    res = aiot_logpost_deinit(&logpost_handle);
    if (res < STATE_SUCCESS)
    {
        printf("aiot_logpost_deinit failed: -0x%04X\r\n", -res);
        return -1;
    }
    return res;
}
