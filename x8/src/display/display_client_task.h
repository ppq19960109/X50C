#ifndef _DISPLAT_CLIENT_TASK_H_
#define _DISPLAT_CLIENT_TASK_H_
#ifdef ICE_ENABLE
int display_send(unsigned char *data, unsigned int len);
void *display_client_task(void *arg);
void display_client_close(void);
#endif
#endif