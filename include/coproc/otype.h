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
#ifndef _OTYPE_H
#define _OTYPE_H

#include <cheri/cherireg.h>
#include <sys/cdefs.h>

/* 
 * PBB: It is my belief that this will probably be more otype space than the 
 * microkernel needs. However, as we move to a more modular design, we could
 * find that lots of things make sense as microkernel services.
 */
#define COPROC_USER_OTYPE_MIN		CHERI_OTYPE_USER_MIN
#define COPROC_USER_OTYPE_MAX		(CHERI_OTYPE_USER_MAX >> 1)

#define COPROC_UKERNEL_OTYPE_MIN	(COPROC_USER_OTYPE_MAX + 1)
#define COPROC_UKERNEL_OTYPE_MAX	CHERI_OTYPE_USER_MAX
#define COPROC_UKERNEL_OTYPE_FLAG	COPROC_UKERNEL_OTYPE_MIN

#define COPROC_OTYPE_SPACE_LEN		(CHERI_OTYPE_USER_MAX >> 1)
#define COPROC_OTYPE_ISUKERN(x)		(((x) & COPROC_UKERNEL_OTYPE_FLAG) != 0)
#define COPROC_OTYPE_ISUSER(x)		(!(COPROC_OTYPE_ISUKERN(x)))

#define OTYPE_PERM_UNSEAL			CHERI_PERM_UNSEAL
#define OTYPE_PERM_SEAL				CHERI_PERM_SEAL
#define OTYPE_PERMS_OWN				(CHERI_PERM_SEAL | CHERI_PERM_UNSEAL)

#ifndef _OTYPE_T_DECLARED
#define _OTYPE_T_DECLARED
typedef void * __capability otype_t;
#endif //!defined(_OTYPE_T_DECLARED)

struct object_type
{
	otype_t sc;
	otype_t usc;
	long otype;
};

__BEGIN_DECLS

void *make_otypes(otype_t rootcap, int n_otypes, struct object_type **results);

__END_DECLS


#endif //!defined(_COTYPE_H)