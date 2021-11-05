#ifndef __MAIN_H_
#define __MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/reboot.h>
#include <pthread.h>

#include "cJSON.h"
#include "base64.h"
#include "curl/curl.h"

#include "logFunc.h"
#include "cmd_run.h"
#include "commonFunc.h"
#include "networkFunc.h"

#include "infra_config.h"
#include "infra_types.h"
#include "infra_defs.h"
#include "infra_state.h"
#include "infra_compat.h"
#include "infra_log.h"
#include "dev_model_api.h"
#include "dynreg_api.h"
#include "wrappers.h"
#include "dev_reset_api.h"


#define ETH_NAME "eth0"

#endif
