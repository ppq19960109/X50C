#ifndef __LINK_LOGPOST_H_
#define __LINK_LOGPOST_H_

int link_logopost_init(void *mqtt_handle);
int link_logopost_deinit();
void link_send_log(char *log);
#endif
