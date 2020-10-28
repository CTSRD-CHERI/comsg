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

.set noreorder
.set noat

#include <machine/asm.h>
#include "SYS.h" /* need to put this in the include path somehow */

.text
.globl _C_LABEL(_cosend)
_C_LABEL(_cosend):
	clc		$c3, $idc /* first arg is unsealed port cap */
	clcbi	$c12, %capcall20(cosend_impl)($cgp)
	cjalr	$c12, $c17
	nop
	cjr		$c13 /* return from cinvoke */
	nop
.globl _C_LABEL(_ecosend)
_C_LABEL(_ecosend)

.text
.globl _C_LABEL(_corecv)
_C_LABEL(_corecv):
	clc		$c3, $idc /* first arg to _corecv_impl is unsealed data cap */
	clcbi	$c12, %capcall20(corecv_impl)($cgp)
	cjalr	$c12, $c17
	nop
	cjr		$c13 /* return from cinvoke */
	nop
.globl _C_LABEL(_ecorecv)
_C_LABEL(_ecorecv)

.data

.global szcorecv
szcorecv:
.long _ecorecv-_corecv

.global szcosend
szcorecv:
.long _ecosend-_cosend