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
#include <comsg/coevent.h>

#include <assert.h>
#include <unistd.h>
#include <err.h>
#include <stdlib.h>
#include <sysexits.h>
#include <sys/auxv.h>

static coport_t *cocarrier = NULL;
static coport_t *copipe = NULL;

static void
process_capvec(void)
{
    int error;
    void **capv;
    size_t capc;

    error = elf_aux_info(AT_CAPV, &capv, sizeof(capv));
    error = elf_aux_info(AT_CAPC, &capc, sizeof(capc));
    if (capc < 2)
        err(EX_SOFTWARE, "%s: invalid capvec format", __func__);

    cocarrier = capv[0];
    copipe = capv[1];
}

static void
init_test(void)
{
  int error;
  void *coproc_init_scb;

  process_capvec();
  error = colookup(U_COPROC_INIT, &coproc_init_scb);
  if (error != 0){
      err(EX_SOFTWARE, "%s: comsg microkernel not available", __func__);
  }
  set_ukern_target(COCALL_COPROC_INIT, coproc_init_scb);
  root_ns = coproc_init(NULL, NULL, NULL, NULL);
}

int main(int argc, char const *argv[])
{
	(void)argc;
	(void)argv;
  int error;
	
  coevent_t *my_death = NULL;
  comsg_attachment_t attachment;
  coevent_subject_t subject_death;
  char *buf = calloc(2, sizeof(coevent_t *));

  init_test();
  subject_death.ces_pid = getpid();
  my_death = colisten(PROCESS_DEATH, subject_death);
  if (my_death == NULL) {
    err(EX_SOFTWARE, "%s: failed to start coeventd listening for process death", __func__);
  }

  attachment.item.coevent = my_death;
  attachment.type = ATTACHMENT_COEVENT;
  error = cosend_oob(cocarrier, buf, sizeof(coevent_t *), &attachment, 1);
  if (error < 0)
    err(EX_SOFTWARE, "%s: failed to cosend to coproctest via cocarrier", __func__);
  error = cosend(copipe, buf, sizeof(coevent_t *));
  if (error < 0)
    err(EX_SOFTWARE, "%s: failed to corecv to coproctest via copipe", __func__);
  error = corecv(copipe, (void **)&buf, sizeof(coevent_t *));
  if (error < 0)
    err(EX_SOFTWARE, "%s: failed to corecv from coproctest via copipe", __func__);
	sleep(5);
	sleep(5);

	return (0);
}