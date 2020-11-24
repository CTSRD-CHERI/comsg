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

#include <coproc/utils.h>

#include <assert.h>
#include <cheri/cheric.h>
#include <sys/sysctl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>

static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

int
generate_id(void)
{
    static int id = 1;
    // TODO: Replace this with something smarter.
    return id++;
}

/* these are not secret or secure, they just shouldn't be the same as each other by accident*/

__attribute__((constructor))
void init_rand(void)
{
    int garbage;
    srand((unsigned)garbage);
}

int
rand_string(char *buf, size_t len)
{
    size_t i;
    int rand_no;
    char c;

    
    //last character should contain a NULL
    len = MIN(len + 1, cheri_getlen(buf) - 1);
    for (i = 0; i < len; i++)
    {
        rand_no = rand() % KEYSPACE;
        c = alphanum[rand_no];
        buf[i] = c;
    }
    buf[len] = '\0';
    
    return (len);
}

int
valid_scb(void * scb)
{
    return (1);
}

int 
get_maxprocs(void)
{
    int mib[4] = {CTL_KERN, KERN_MAXPROC, 0, 0};
    int maxprocs;
    size_t len = sizeof(maxprocs);
    sysctl(mib, 2, &maxprocs, &len, NULL, 0);
    return (maxprocs);
}