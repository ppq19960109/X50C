
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"
#include "core_mqtt.h"
#include "core_global.h"

#include "link_solo.h"
#include "KV_linux.h"
#define LINK_RST_FLAG "link_reset"
static char reset_topic_fmt_buf[128];

static void link_reset_recv_handler(void *handle, const aiot_mqtt_recv_t *packet, void *userdata)
{
    char rst = 0x00;
    H_Kv_Set(LINK_RST_FLAG, &rst, sizeof(rst), 0);
    printf("type:%d\n", packet->type);
    printf("topic:%s\n", packet->data.pub.topic);
    printf("payload:%s\n", packet->data.pub.payload);
}

int link_reset_init(void *mqtt_handle, const char *productkey, const char *devicename)
{
    const char *reset_fmt = "/sys/%s/%s/thing/reset";
    const char *reset_reply_fmt = "/sys/%s/%s/thing/reset_reply";
    sprintf(reset_topic_fmt_buf, reset_fmt, productkey, devicename);
    char fmt_buf[128];
    sprintf(fmt_buf, reset_reply_fmt, productkey, devicename);
    aiot_mqtt_sub(mqtt_handle, fmt_buf, link_reset_recv_handler, 1, NULL);
    return 0;
}
static int link_reset(void)
{
    if (get_link_connected_state() == 0)
        return -1;
    int32_t alink_id;

    const char *payload_fmt = "{\"id\":%d, \"version\":\"1.0\", \"method\":\"thing.reset\", \"params\":{}}";
    void *mqtt_handle = get_mqtt_handle();
    if (mqtt_handle == NULL)
        return -1;
    core_mqtt_handle_t *handle = (core_mqtt_handle_t *)mqtt_handle;
    int res = core_global_alink_id_next(handle->sysdep, &alink_id);
    if (res != STATE_SUCCESS)
    {
        printf("link_reset_report error:%d\n", res);
        return -1;
    }

    char payload_fmt_buf[128];
    sprintf(payload_fmt_buf, payload_fmt, alink_id);

    aiot_mqtt_pub(mqtt_handle, reset_topic_fmt_buf, (unsigned char *)payload_fmt_buf, strlen(payload_fmt_buf), 1);
    return 0;
}

int link_reset_report(void)
{
    char rst = 0x01;
    H_Kv_Set(LINK_RST_FLAG, &rst, sizeof(rst), 0);
    link_reset();
    return 0;
}

int link_reset_check(void *mqtt_handle)
{
    int len = 1;
    char rst = 0;
    int ret = H_Kv_Get(LINK_RST_FLAG, &rst, &len);
    if (ret < 0)
    {
        rst = 1;
        H_Kv_Set(LINK_RST_FLAG, &rst, sizeof(rst), 0);
    }
    if (rst > 0)
        link_reset();
    return 0;
}

void link_reset_deinit(void *mqtt_handle)
{
    aiot_mqtt_unsub(mqtt_handle, reset_topic_fmt_buf);
}
