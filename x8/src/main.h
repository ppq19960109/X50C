#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/reboot.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/un.h>

#include "cJSON.h"
#include "base64.h"
#include "curl/curl.h"

#include "cmd_run.h"
#include "commonFunc.h"
#include "networkFunc.h"
#include "signalQuit.h"
#include "POSIXTimer.h"
#include "UartCfg.h"

#include "hloop.h"
#include "hbase.h"
#include "hlog.h"
#include "nlog.h"
#include "hsocket.h"
#include "hssl.h"
#include "hmain.h"
#include "hthread.h"
extern hloop_t *g_loop;
void mlog_hex(const void *buf, int len, const char *file, const int line, const char *func);
#define mlogHex(buf, buf_len) mlog_hex(buf, buf_len, __FILENAME__, __LINE__, __FUNCTION__)

// #define ICE_ENABLE
#ifdef ICE_ENABLE
#include "display_client_task.h"
#endif
#endif
