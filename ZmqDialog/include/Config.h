#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

namespace ZmqDialog
{
#define PAGE_SIZE                   4096
#define MAX_EXCEPTION_MSG_LENGTH    1024
#define DLG_SERVER_TCP_PORT         55550
#define TIMEOUT_INTERVAL            2500000
#define HEARTBEAT_INTERVAL          2500000 // usecs
}

#endif
