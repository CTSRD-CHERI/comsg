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



static int 
coport_event_match(coport_t *port, coport_eventmask_t event)
{
	if (port->event & e)
		return (1);
	else
		return (0);
}

void *
copoll_deliver(void *args)
{
	size_t idx;
	coport_t **cocarrier_array, *cocarrier;
	coport_listener_t *listener, *temp_listener;
	coport_eventmask_t revents;

	acquire_copoll_mutex();
	for (;;) {
		await_copoll_events();
		cocarrier_array = walk_cocarrier_table(modulo, remainder);
		if (cocarrier_array == NULL)
			continue;
		cocarrier = cocarrier_array[0];
		while (cocarrier != NULL) {
			LIST_FOREACH(listener, &cocarrier->cd->listeners, entries) {
				revents = (cocarrier->info->event & listener->events);
				listener->revents = revents;
				if(revents == NOEVENT) 
					continue;
				pthread_cond_signal(&listener->wakeup);
			}
			cocarrier = cocarrier_array[++idx];
		}
		free(cocarrier_array);
	}
	release_copoll_mutex();
	return (NULL);
}
