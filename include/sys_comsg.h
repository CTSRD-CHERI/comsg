/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
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
#define U_COPOLL "copoll"
#define U_COMMAP "commap"
#define U_COMUNMAP "comunmap"

#define U_SOCKADDR "getukernsockaddr" //Currently doesn't use requests interface

#define U_FUNCTIONS 10 //coclose not yet implemented

#define MAX_COPORTS 10
#define LOOKUP_STRING_LEN 16
#define COPORT_BUF_LEN 4096
#define COPORT_NAME_LEN 255
#define COMUTEX_NAME_LEN 255
#define UKERN_OTYPE 2
#define COCARRIER_OTYPE ( UKERN_OTYPE )
#define COCARRIER_SIZE ( COPORT_BUF_LEN / CHERICAP_SIZE )
#define COCARRIER_MAX_MSGLEN (  )

#endif