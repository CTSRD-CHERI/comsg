/*system-wide colocation constants*/
#ifndef _SYS_COMSG_H
#define _SYS_COMSG_H

#include <cheri/cherireg.h>

#define U_COOPEN "coopen"
#define U_COCLOSE "coclose"
#define U_COUNLOCK "counlock"
#define U_COLOCK "colock"
#define U_COMUTEX_INIT "comutex_init"
#define U_COCARRIER_SEND "cocarrier_send"
#define U_COCARRIER_RECV "cocarrier_recv"
#define U_COCARRIER_POLL "copoll"

#define U_FUNCTIONS 8

#define MAX_COPORTS 10
#define LOOKUP_STRING_LEN 16
#define COPORT_BUF_LEN 4096
#define COPORT_NAME_LEN 255
#define COMUTEX_NAME_LEN 255
#define COCARRIER_SIZE ( COPORT_BUF_LEN / CHERICAP_SIZE )


#endif