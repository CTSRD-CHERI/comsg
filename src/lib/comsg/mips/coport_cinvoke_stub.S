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

#define _MIPS_REGDEF_H
#include <machine/asm.h>

#include <cheri/cherireg.h>

/* CCall-and-link-register */
#define CCALLALR(cb, cd)						\
	.set push;						\
	.set noreorder;						\
	cgetpcc $c17;						\
	cincoffset $c17, $c17, 12;			\
	ccall cb, cd, 1;					\
	nop; 	\
	.set pop;


#define CINVOKE_FRAME_SIZE (11*CHERICAP_SIZE + 96)


/* XXX-PBB: register save/restore from cheri_invoke  */
/*
 * int 
 * coport_cinvoke(void *codecap, coport_t *coport, void *buf, const void *ret_sealcap, size_t len)
 */
.text 
.option pic0
NESTED(coport_cinvoke, CINVOKE_FRAME_SIZE, _FRAME_RETURN_REG)
	cgetoffset $t0, $csp
	cincoffset	$csp, $csp, -96
	csd	$s0, $zero, 0($csp)
	csd	$s1, $zero, 8($csp)
	csd	$s2, $zero, 16($csp)
	csd	$s3, $zero, 24($csp)
	csd	$s4, $zero, 32($csp)
	csd	$s5, $zero, 40($csp)
	csd	$s6, $zero, 48($csp)
	csd	$s7, $zero, 56($csp)
	csd	$gp, $zero, 64($csp)
	csd	$fp, $zero, 72($csp)
	csd	$ra, $zero, 80($csp)

	cgetdefault	$c12
	cincoffset	$csp, $csp, -11*CHERICAP_SIZE

	csc	$c17, $zero, 0*CHERICAP_SIZE($csp)
	csc	$c18, $zero, 1*CHERICAP_SIZE($csp)
	csc	$c19, $zero, 2*CHERICAP_SIZE($csp)
	csc	$c20, $zero, 3*CHERICAP_SIZE($csp)
	csc	$c21, $zero, 4*CHERICAP_SIZE($csp)
	csc	$c22, $zero, 5*CHERICAP_SIZE($csp)
	csc	$c23, $zero, 6*CHERICAP_SIZE($csp)
	csc	$c24, $zero, 7*CHERICAP_SIZE($csp)
	csc	$c25, $zero, 8*CHERICAP_SIZE($csp)
	csc	$c26, $zero, 9*CHERICAP_SIZE($csp)
	csc	$c12, $zero, 10*CHERICAP_SIZE($csp)

	/* Load argument registers */
	cmove $c1, $c3
	cmove $c2, $c4
	cmove $c4, $c5
	/* 
	 * $c3 is currently restored by inline assembly 
	 * future abi ought to handle this for us
	 */

	cmove $c3, $cnull
	cmove $c5, $cnull
	/* Prepare ccall-return registers */
	
	cseal $c21, $csp, $c6 /* idc to restore on return */
	cgetoffset $t1, $csp
	csetoffset $csp, $csp, $zero /* now prevent callee access to lower frames than the current one */
	csetbounds $csp, $csp, $t0
	csetoffset $csp, $csp, $t1
	

	cgetpcc $c22 /* return address, c17 will not work as it is saved by compiler generated code */
	cincoffset $c22, $c22, 20
	cseal $c22, $c22, $c6
	cmove $c6, $cnull
	
	ccall $c1, $c2, 1
	nop

XNESTED(cinvoke_return)
	cmove $csp, $idc

	clc	$c17, $zero, 0($csp)
	clc	$c18, $zero, CHERICAP_SIZE($csp)
	clc	$c19, $zero, 2*CHERICAP_SIZE($csp)
	clc	$c20, $zero, 3*CHERICAP_SIZE($csp)
	clc	$c21, $zero, 4*CHERICAP_SIZE($csp)
	clc	$c22, $zero, 5*CHERICAP_SIZE($csp)
	clc	$c23, $zero, 6*CHERICAP_SIZE($csp)
	clc	$c24, $zero, 7*CHERICAP_SIZE($csp)
	clc	$c25, $zero, 8*CHERICAP_SIZE($csp)
	clc	$c26, $zero, 9*CHERICAP_SIZE($csp)
	clc	$c12, $zero, 10*CHERICAP_SIZE($csp)
	csetdefault	$c12
	cincoffset	$csp, $csp, 11*CHERICAP_SIZE

	cld	$s0, $zero, 0($csp)
	cld	$s1, $zero, 8($csp)
	cld	$s2, $zero, 16($csp)
	cld	$s3, $zero, 24($csp)
	cld	$s4, $zero, 32($csp)
	cld	$s5, $zero, 40($csp)
	cld	$s6, $zero, 48($csp)
	cld	$s7, $zero, 56($csp)
	cld	$gp, $zero, 64($csp)
	cld	$fp, $zero, 72($csp)
	cld	$ra, $zero, 80($csp)
	cincoffset	$csp, $csp, 96

	cjr	$c17
	nop
END(coport_cinvoke)


