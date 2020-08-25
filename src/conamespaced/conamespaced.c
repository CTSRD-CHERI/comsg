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

/*
 * Functions provided:
 * create child namespace - requires ownership of the parent namespace
 * create object within specified namespace - requires write permission to the namespace 
 * list objects within specified namespace - requires read permission to the namespace 
 * retrieve handle to named object from specified namespace - requires read permission to the namespace
 * delete object from namespace - requires ownership of the object and write permission to the namespace; or ownership of the namespace
 * delete namespace - requires ownership; certain types are deleted by the system.
 * 
 */

/* 
 * Responsibilities:
 * track lifetime of coprocess-aware processes and threads inside them
 * 	+ creates and deletes namespaces for them 
 * 	+ manages the global namespace
 * 	+ performs cleanup on thread/program exit
 * 		+ deletes coservices provided by dead threads
 */

#include "nsd_daemons.h"

#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

static namespace_t *global_ns;


int main(int argc, char const *argv[])
{
	//we can dance if we want to
	global_ns = create_namespace("coproc", GLOBAL, NULL);
	
	return (0);
}