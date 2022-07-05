#ifndef __LINK_DYNREGMQ_H_
#define __LINK_DYNREGMQ_H_

#define DYNREGMQ

void register_dynreg_device_secret_cb(int (*cb)(const char *));
int link_dynregmq_start(const char *mqtt_host, const char *product_key, const char *product_secret, const char *device_name, char *device_secret);
#endif
