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
#ifndef _UKERN_PARAMS_H
#define _UKERN_PARAMS_H

#define WORKER_COUNT 1
#define THREAD_STRING_LEN 16

#define MAX_COMUTEXES 20
#define MAX_COPOLL 255

#define UKERN_PRIV 1

//TODO-PBB: revise
const int nworkers = 12;



#define MAX_COPORTS 10

#define COPORT_BUF_LEN 4096
#ifndef NS_NAME_LEN
#define LOOKUP_STRING_LEN 255
#define COPORT_NAME_LEN 255
#define COMUTEX_NAME_LEN 255
#else
#define LOOKUP_STRING_LEN NS_NAME_LEN
#define COPORT_NAME_LEN NS_NAME_LEN
#define COMUTEX_NAME_LEN NS_NAME_LEN
#endif
#define UKERN_OTYPE 2
#define COCARRIER_OTYPE ( UKERN_OTYPE )
#define COCARRIER_SIZE ( COPORT_BUF_LEN / CHERICAP_SIZE )
#define COCARRIER_MAX_MSGLEN (  )

#endif