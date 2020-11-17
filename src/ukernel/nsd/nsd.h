/*
 * Copyright (c) 2020 Peter S. Blandford-Baker
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
#ifndef _NSD_H
#define _NSD_H

#ifndef COPROC_UKERN
#define COPROC_UKERN 1
#endif

#include <cocall/cocall_args.h>
#include <cocall/worker_map.h>
#include <coproc/coservice.h>


extern coservice_provision_t coinsert_serv, coselect_serv, coupdate_serv, codelete_serv, cocreate_serv, codrop_serv;


void namespace_object_update(coupdate_args_t *cocall_args, void *token);
void namespace_object_delete(codelete_args_t *cocall_args, void *token);
void namespace_create(cocreate_args_t *cocall_args, void *token);
void namespace_drop(codrop_args_t *cocall_args, void *token);
void namespace_object_insert(coinsert_args_t *cocall_args, void *token);
void namespace_object_select(coselect_args_t * cocall_args, void *token);

int validate_coselect_args(coselect_args_t *cocall_args);
int validate_coupdate_args(coupdate_args_t *cocall_args);
int validate_codelete_args(codelete_args_t *cocall_args);
int validate_cocreate_args(cocreate_args_t *cocall_args);
int validate_codrop_args(codrop_args_t *cocall_args);
int validate_coinsert_args(coinsert_args_t *cocall_args);


//TODO-PBB: revisit
#define NSD_NWORKERS 12

#endif //!defined(_NSD_H)