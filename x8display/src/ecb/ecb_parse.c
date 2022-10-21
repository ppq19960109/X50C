#include "main.h"

#include "ecb_parse.h"
#include "ecb_uart.h"
#include "ecb_uart_parse_msg.h"

static unsigned char ecb_set_state[26];

static ecb_attr_t g_event_state[] = {
    {
        uart_cmd : 0x03,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x04,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x05,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x0a,
        uart_byte_len : 4,
    },
    {
        uart_cmd : 0x0b,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x11,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x12,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x13,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x14,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x31,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x32,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x34,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x38,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x42,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x43,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x44,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x45,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x46,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x47,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x48,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x49,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x4a,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x4b,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x4f,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x50,
        uart_byte_len : 4,
    },
    {
        uart_cmd : 0x52,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x53,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x54,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x55,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x56,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x57,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x58,
        uart_byte_len : 2,
    },
    {
        uart_cmd : 0x59,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x5a,
        uart_byte_len : 1,
    },
    {
        uart_cmd : 0x5b,
        uart_byte_len : 1,
    },
};

static g_event_state_len = sizeof(g_event_state) / sizeof(g_event_state[0]);

ecb_attr_t *get_event_state(signed short uart_cmd)
{
    for (int i = 0; i < g_event_state_len; ++i)
    {
        if (uart_cmd == g_event_state[i].uart_cmd)
        {
            return &g_event_state[i];
        }
    }
    return NULL;
}

int ecb_parse_event_msg(unsigned char *data, unsigned int len)
{
    if (len < 26)
        return -1;

    int index = 0;
    if (data[index + 0] != 0xf5 || data[index + 1] != 0x19)
    {
        return -1;
    }
    ecb_attr_t *ecb_attr;
    ecb_attr = get_event_state(EVENT_SYSPOWER);
    *ecb_attr->value = data[index + 4];

    return 0;
}
static int ecb_parse_set_cmd(unsigned char cmd, unsigned char *value)
{

    return 0;
}
int ecb_parse_set_msg(unsigned char *data, unsigned int len)
{

    return 0;
}
void ecb_parse_init()
{
    for (int i = 0; i < g_event_state_len; ++i)
    {
        if (g_event_state[i].uart_byte_len > 0)
        {
            g_event_state[i].value = (char *)malloc(g_event_state[i].uart_byte_len);
            if (g_event_state[i].value == NULL)
            {
                dzlog_error("g_event_state[%d].value malloc error\n", i);
                return;
            }
            memset(g_event_state[i].value, 0, g_event_state[i].uart_byte_len);
        }
    }
}
void ecb_parse_deinit()
{
    for (int i = 0; i < g_event_state_len; ++i)
    {
        if (g_event_state[i].uart_byte_len > 0)
            free(g_event_state[i].value);
    }
}