#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "infra_config.h"
#include "infra_state.h"
#include "infra_types.h"
#include "infra_defs.h"
#include "infra_compat.h"
#include "dev_reset_api.h"
#include "dev_model_api.h"
#include "wrappers.h"

#include "linkkit_solo.h"

#define LINKKIT_RST "awss.rst" //"awss.rst" linkkit_rst

static void linkkit_devrst_evt_handle(iotx_devrst_evt_type_t evt, void *msg)
{
    switch (evt)
    {
    case IOTX_DEVRST_EVT_RECEIVED:
    {
        iotx_devrst_evt_recv_msg_t *recv_msg = (iotx_devrst_evt_recv_msg_t *)msg;

        EXAMPLE_TRACE("Receive Reset Responst");
        EXAMPLE_TRACE("Msg ID: %d", recv_msg->msgid);
        EXAMPLE_TRACE("Payload: %.*s", recv_msg->payload_len, recv_msg->payload);

        char rst = 0x00;
        HAL_Kv_Set(LINKKIT_RST, &rst, sizeof(rst), 0);
    }
    break;

    default:
        break;
    }
}

static int linkkit_dev_reset(void)
{
    int res = 0;
    iotx_dev_meta_info_t reset_meta_info;
    memset(&reset_meta_info, 0, sizeof(iotx_dev_meta_info_t));
    get_linkkit_quad(reset_meta_info.product_key, NULL, reset_meta_info.device_name, NULL);
    if (strlen(reset_meta_info.product_key) == 0 || strlen(reset_meta_info.device_name) == 0)
        return -1;

    res = IOT_DevReset_Report(&reset_meta_info, linkkit_devrst_evt_handle, NULL);
    if (res < 0)
    {
        EXAMPLE_TRACE("IOT_DevReset_Report Failed\n");
        return -1;
    }
    return res;
}

int linkkit_unbind(void)
{
    char rst = 0x01;
    HAL_Kv_Set(LINKKIT_RST, &rst, sizeof(rst), 0);

    return linkkit_dev_reset();
}

int linkkit_unbind_check(void)
{
    int len = 1;
    char rst = 0;
    int ret;
    ret = HAL_Kv_Get(LINKKIT_RST, &rst, &len);
    if (ret < 0)
    {
        EXAMPLE_TRACE("linkkit_unbind_check %s not exits\n", LINKKIT_RST);
        return -1;
    }
    EXAMPLE_TRACE("linkkit_unbind_check rst:%d", rst);
    if (rst == 0)
    {
        return -1;
    }
    return linkkit_dev_reset();
}
