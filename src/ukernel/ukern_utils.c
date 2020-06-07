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

#include "ukern_utils.h"

#include <cheri/cheric.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/param.h>
#include <machine/sysarch.h>
#include <sys/sysctl.h>
#include <unistd.h>

static int multicore = 0;

static int id = 1;
int generate_id(void)
{
    // TODO: Replace this with something smarter.
    return id++;
}

__attribute__ ((constructor)) static 
void setup_utils(void)
{
    int mib[4];
    int cores;
    size_t len = sizeof(cores); 

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &cores, &len, NULL, 0);
    if (cores>1)
        multicore=1;
}

inline
int is_multicore(void)
{
    return multicore;
}

int rand_string(char * buf, size_t len)
{
    char c;
    int rand_no;
    //last character should contain a NULL
    len=MIN(len+1,cheri_getlen(buf)-1);
    srandomdev();
    for (size_t i = 0; i < len; i++)
    {
        rand_no=random() % KEYSPACE;
        if (rand_no<10)
            c=(char)rand_no+'0';
        else if (rand_no<36)
            c=(char)(rand_no % 26)+'A';
        else 
            c=(char)(rand_no % 26)+'a';
        buf[i]=c;
    }
    buf[len]='\0';
    return len;
}

void debug_noop(int i)
{
    if(i>0 && 0)
        i++;
    return;
}

