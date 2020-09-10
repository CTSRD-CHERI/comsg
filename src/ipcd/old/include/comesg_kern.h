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
#ifndef _COMESG_KERN
#define _COMESG_KERN

#include <pthread.h>
#include <stdatomic.h>
#include <cheri/cheric.h>
#include <cheri/cherireg.h>
#include <stdbool.h>
#include <sys/queue.h>

#include "coport.h"
#include "sys_comsg.h"
#include "sys_comutex.h"
#include "ukern_params.h"



/*#define TBL_PERMS ( CHERI_PERM_LOAD | CHERI_PERM_LOAD_CAP | \
	CHERI_PERM_STORE | CHERI_PERM_STORE_CAP | CHERI_PERM_GLOBAL |\
	CHERI_PERM_STORE_LOCAL_CAP )*/
#define WORKER_FUNCTIONS ( U_FUNCTIONS + UKERN_PRIV )

void *copoll_deliver(void *args);
void *cocarrier_poll(void *args);
void *cocarrier_register(void *args);
void *cocarrier_recv(void *args);
void *cocarrier_send(void *args);
void *coport_open(void *args);
int main(int argc, const char *argv[]);

extern otype_t seal_cap;
extern long sealed_otype;


#endif