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

#include <sys/select.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "zlog.h"

#include "cJSON.h"
#include "base64.h"
#include "curl/curl.h"


#include "cmd_run.h"
#include "commonFunc.h"
#include "networkFunc.h"
#include "signalQuit.h"
#include "POSIXTimer.h"
#include "UartCfg.h"

#endif
