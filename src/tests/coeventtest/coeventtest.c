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
#include <comsg/coport_ipc.h>
#include <comsg/ukern_calls.h>
#include <coproc/coevent.h>

#include <unistd.h>
#include <err.h>
#include <sysexits.h>
#include <sys/auxv.h>

static coport_t *copipe;

static void
process_capvec(void)
{
    int error;
    void **capv;

    error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
    if (capv[1] != NULL)
        err(EX_SOFTWARE, "%s: invalid capvec format", __func__);

    copipe = capv[0];
}

int main(int argc, char const *argv[])
{
	(void)argc;
	(void)argv;
    int error;
	void *coproc_init_scb;
    coevent_t *my_death;
    coevent_subject_t subject_death;

    error = colookup(U_COPROC_INIT, &coproc_init_scb);
    if (error != 0){
        err(EX_SOFTWARE, "%s: comsg microkernel not available", __func__);
    }
    set_ukern_target(COCALL_COPROC_INIT, coproc_init_scb);
    global_ns = coproc_init(NULL, NULL, NULL, NULL);
    subject_death.ces_pid = 0; //NOTUSED
    my_death = colisten(PROCESS_DEATH, subject_death);
    error = cosend(copipe, &my_death, sizeof(my_death));
	sleep(5);
	sleep(5);

	return (0);
}