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
#ifndef _NSD_H
#define _NSD_H

#ifndef COPROC_UKERN
#define COPROC_UKERN 1
#endif

#include "ukern/coservice.h"
#include "ukern/worker_map.h"

extern coservice_provision_t coinsert_serv, coselect_serv, coupdate_serv, codelete_serv, cocreate_serv, codrop_serv;


void update_namespace_object(coupdate_args_t *cocall_args, void *token);
void delete_namespace_object(codelete_args_t *cocall_args, void *token);
void create_namespace(cocreate_args_t *cocall_args, void *token);
void drop_namespace(codrop_args_t *cocall_args, void *token);
void insert_namespace_object(coinsert_args_t *cocall_args, void *token);
void select_namespace_object(coselect_args_t * cocall_args, void *token);

int validate_coselect_args(coselect_args_t*);
int validate_coupdate_args(coupdate_args_t *cocall_args);
int validate_codelete_args(codelete_args_t *cocall_args);
int validate_cocreate_args(cocreate_args_t *cocall_args);
int validate_codrop_args(codrop_args_t *cocall_args);


//TODO-PBB: revisit
extern const int nworkers = 12;

#endif //!defined(_NSD_H)