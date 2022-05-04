#ifndef __LINK_REMOTE_ACCESS_H_
#define __LINK_REMOTE_ACCESS_H_

// #define REMOTE_ACCESS
#ifdef REMOTE_ACCESS
int link_remote_access_open(void *mqtt_handle, void *cred);
int link_remote_access_close(void);
#endif
#endif
