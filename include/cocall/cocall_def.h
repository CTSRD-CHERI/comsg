/*
 * Copyright (c) 2021 Peter S. Blandford-Baker
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
#ifndef _COCALL_FUNCTION_H
#define _COCALL_FUNCTION_H

#include <sys/queue.h>

struct _scb_entry {
	SLIST_ENTRY(_scb_entry) entries;
	void *scb;
};

SLIST_HEAD(scb_list, _scb_entry);

#define DECL_COCALL(name) \
	extern struct scb_list name##_scb_list;\
	extern thread_local struct _scb_entry *cur_##name_scb;

#define CUR_SCB_ENTRY(name) (cur_##name_scb)
#define CUR_SCB(name) (cur_##name_scb->scb)
#define SCB_LIST(name) (&##name_scb_list)

/* TODO-PBB: address thread safety issues */
#define FIRST_SCB_ENTRY(name) SLIST_FIRST(SCB_LIST(name))
#define NEXT_SCB_ENTRY(name) SLIST_NEXT(CUR_SCB(name), entries)
#define ADD_SCB_ENTRY(name, scb_entry) SLIST_INSERT_HEAD(SCB_LIST(name), scb_entry, entries)


#endif //!defined(_COCALL_FUNCTION_H)