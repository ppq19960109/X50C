#ifndef _ICE_PARSE_H_
#define _ICE_PARSE_H_

#include <stdbool.h>
#define RStOvMode_ICE (120)
enum msg_get_time_t
{
    MSG_GET_SHORT_TIME = 3 * 7,
    MSG_GET_LONG_TIME = 4 * 30 * 7,
};
enum ice_event_set_state_enum
{
    EVENT_SET_IceStOvMode = 0X71,
    EVENT_IceStOvState = 0X72,
    EVENT_SET_IceStOvSetTemp = 0X73,
    EVENT_SET_IceStOvRealTemp = 0X74,
    EVENT_SET_IceStOvSetTimer = 0X75,
    EVENT_IceStOvSetTimerLeft = 0X76,
    EVENT_IceHoodSpeed = 0X77,
    SET_IceStOvOperation = 0X78,
    EVENT_IceHoodLight = 0X79,
};
typedef struct
{
    unsigned char iceStOvMode;
    unsigned char iceStOvState;
    unsigned short iceStOvSetTemp;
    unsigned short iceStOvRealTemp;
    unsigned short iceStOvSetTimer;
    unsigned short iceStOvSetTimerLeft;
    unsigned char iceHoodSpeed;
    unsigned char iceHoodLight;
    unsigned char first_ack;
} ice_event_state_t;

extern ice_event_state_t g_ice_event_state;

int ice_uart_msg_get(bool increase);
int ice_uart_parse_msg(const unsigned char *in, const int in_len, int *end);
int ice_uart_send_event_msg(unsigned char *msg, const int msg_len);

int ice_uart_send_set_real_temp(unsigned short temp);
int ice_uart_send_set_msg(unsigned char operation, unsigned char mode, unsigned short temp, unsigned short time);
#endif