#ifndef _ECB_PARSE_H_
#define _ECB_PARSE_H_

typedef struct
{
    signed short uart_cmd;
    unsigned char uart_byte_len;
    char *value;
} ecb_attr_t;

enum event_state_enum
{
    EVENT_SYSPOWER=0X05,

};
enum set_state_enum
{
    SET_SYSPOWER=0X05,
    
};
#endif