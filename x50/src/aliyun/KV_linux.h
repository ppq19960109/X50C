#ifndef _KV_LINUX_H_
#define _KV_LINUX_H_

// #include "infra_types.h"
// #include "infra_defs.h"

int H_Kv_Set(const char *key, const void *val, int len, int sync);
int H_Kv_Get(const char *key, void *val, int *buffer_len);
int H_Kv_Del(const char *key);

#endif

