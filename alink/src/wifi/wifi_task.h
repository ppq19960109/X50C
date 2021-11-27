#ifndef _WIFI_TASK_H_
#define _WIFI_TASK_H_

#include "cloud_process.h"

typedef struct
{
    char cloud_key[33];
    enum LINK_FUN_TYPE fun_type;
    void *(*cb)(void *, void *);

    union
    {
        char c;
        int n;
        double f;
        char *p;
    } value;
} set_attr_t;
int wifi_recv_from_uds(cJSON *root, const char *type);
#endif