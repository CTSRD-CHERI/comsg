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

#include <coproc/module.h>

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>


static size_t cur_ndeps = 0;
static struct _provider *deps = NULL;

struct depq_entry {
	struct _provider *provider;
	TAILQ_ENTRY(depq_entry) depq_next;
};

static TAILQ_HEAD(, depq_entry) deps = 
	TAILQ_HEAD_INITIALIZER(deps);

static bool module_init_complete = false;

static coproc_deps_initializer void
init_deps_array(void)
{
	return;
}

void
add_dependency(char *provider_module, char *provider_submodule)
{
	struct depq_entry *entry;
	struct _provider *prov;
	/* placeholder for when i'm in less dental pain */
	/* should only be called from constructors with priority greater than register_dependencies */
	assert(!module_init_complete);
	/* check dependency not installed */
	TAILQ_FOREACH(entry, &deps, depq_next) {
		prov = entry->provider;
		if (strcmp(prov->module, provider_module) == 0) {
			if (provider_submodule == NULL && prov->submodule == NULL)
				return;
			else if (provider_submodule != NULL && prov->submodule != NULL) {
				if (strcmp(prov->submodule, provider_submodule) == 0)
					return; /* exists */
			} 
		}
	}
	entry = calloc(1, sizeof(struct depq_entry))
	prov = calloc(1, sizeof(struct _provider));
	prov->module = strdup(provider_module);
	if (provider_submodule != NULL)
		prov->submodule = strdup(provider_submodule);
	entry->provider = prov;

	TAILQ_INSERT_TAIL(&deps, entry, depq_next);
	cur_ndeps++;
}

static coproc_deps_register void
register_dependencies(void)
{
	module_init_complete = true;
	if (cur_ndeps != 0)
		//dostuff
}