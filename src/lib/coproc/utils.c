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

#include <coproc/utils.h>

#include <cheri/cheric.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/types.h>
#include <unistd.h>


int generate_id(void)
{
    static int id = 1;
    // TODO: Replace this with something smarter.
    return id++;
}

int rand_string(char *buf, size_t len)
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

int valid_scb(void * scb)
{
    return 1;
}

static __inline otype_t
make_sealcap(void * __capability r, register_t t)
{
    return (cheri_maketype(r, t));
}

static __inline otype_t
make_unsealcap(void *root_type, register_t type)
{
    /* XXX: copied and tweaked minutely from cheri/cheric.h */
    void * __capability c;

    c = root_type;
    c = cheri_setoffset(c, type);   /* Set type as desired. */
    c = cheri_setbounds(c, 1);  /* ISA implies length of 1. */
    c = cheri_andperm(c, CHERI_PERM_GLOBAL | CHERI_PERM_UNSEAL); /* Perms. */
    return (c);
}

static void
make_otype(void *root_type, register_t type, struct object_type *result)
{
    result->sc = make_sealcap(root_type, type);
    result->usc = make_unsealcap(root_type, type);
    result->otype = cheri_gettype(cheri_seal(result, result->sc));
    return (result);
}

void *make_otypes(void * rootcap, int n_otypes, struct object_type **results)
{
    struct object_type new_otype;
    assert(cheri_getlen(rootcap) <= n_otypes);
    assert(cheri_getperm(rootcap) & ( CHERI_PERM_SEAL | CHERI_PERM_UNSEAL ));

    void *seal_root = cheri_setboundsexact(rootcap, i);
    for(int i = 0; i < n_otypes; i++)
        make_otype(seal_root, i, results[i]);

    return (cheri_incoffset(rootcap, i));
}

int get_maxprocs(void)
{
    int mib[4] = {CTL_KERN, KERN_MAXPROC, 0, 0};
    int maxprocs;
    size_t len = sizeof(maxprocs);
    sysctl(mib, 2, &maxprocs, &len, NULL, 0);
    return (maxprocs);
}