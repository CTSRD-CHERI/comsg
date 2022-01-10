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

#include <cocall/worker_map.h>
#include <comsg/ukern_calls.h>
#include <comsg/coport_ipc.h>
#include <comsg/coport_ipc_cinvoke.h>
#include <coproc/namespace.h>
#include <coproc/namespace_object.h>
#include <coproc/coport.h>

#include <assert.h>
#include <err.h>
#include <machine/sysarch.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <sysexits.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <unistd.h>

static char *coprocd_args[] = {"/usr/bin/coprocd", NULL};
extern char **environ;
static void *sealroot;
static pid_t monitored_child;

static const char default_name[] = "coproctestA";
static char test_str[] = "Testing...";

typedef struct {
	coport_t *port;
	void **dest_buf;
	size_t len;
	int *result_ptr;
} corecv_thr_args_t;

pthread_mutex_t start;
pthread_cond_t started;

pthread_mutex_t procdeath;
pthread_cond_t proc_died;

static void 
ccb_example(cocall_args_t *cocall_args, void *token)
{
	(void)(token);
	(void)(cocall_args);

	pthread_mutex_lock(&procdeath);
	pthread_cond_signal(&proc_died);
	pthread_mutex_unlock(&procdeath);
}

static void
do_procdeath_test(void)
{
	function_map_t *fmap;
	coevent_subject_t subject_death;
	char *child_args[] = {"/usr/bin/coeventtest", NULL};
	pid_t child_pid, my_pid;
	cocall_args_t args;
	struct cocallback_args ccb_args;
	coport_t *copipe;
	coevent_t *death;
	int error;

	void **scbs, *scb;
	void **capv;

	fmap = spawn_slow_worker(NULL, ccb_example, NULL);
	scbs = get_worker_scbs(fmap);
	copipe = open_coport(COPIPE);
	scb = scbs[0];
	capv = calloc(2, sizeof(void *));
	capv[0] = scb;
	capv[1] = NULL;

	cocallback_func_t *ccb_func = ccb_register(scb, FLAG_SLOCALL);
	
	my_pid = getpid();
	child_pid = vfork();
	if (child_pid == 0) {
		coexecvec(my_pid, child_args[0], child_args, environ, capv);
		_exit(EX_UNAVAILABLE);
	} 

	pthread_mutex_lock(&procdeath);
	error = corecv(copipe, &death, sizeof(death));
	assert(error > 0);
	ccb_args.len = sizeof(cocall_args_t);
	ccb_args.cocall_data = (void *)&args;
	ccb_install(ccb_func, &ccb_args, death);
	pthread_cond_wait(&proc_died, &procdeath);
	printf("coproctest: coeventd test passed\n");
	pthread_mutex_unlock(&procdeath);
	

}

static void *
do_copipe_recv(void *argp)
{
	corecv_thr_args_t *args = argp;
	*args->result_ptr = corecv(args->port, args->dest_buf, args->len);
	return (NULL);
}

static void *
do_cocarrier(void *argp)
{
	pollcoport_t pct;
	int error;
	char **result_str;
	corecv_thr_args_t *args = argp;

	pthread_mutex_lock(&start);
	make_pollcoport(&pct, args->port, COPOLL_IN);
	printf("coproctest: polling coport (is recv possible?)...\n");
	pthread_cond_signal(&started);
	pthread_mutex_unlock(&start);
	error = copoll(&pct, 1, -1);
	if (error == -1) {
		printf("\t\t\tfailed\n");
		err(errno, "do_cocarrier: copoll failed");
	} else
		printf("coproctest: polling coport... \t\t\tsuccess!\n");
	pthread_mutex_lock(&start);
	printf("coproctest: receiving message...");
	error = corecv(args->port, args->dest_buf, cheri_getlen(test_str));
	*args->result_ptr = cheri_getlen(*args->dest_buf);
	result_str = (char **)args->dest_buf;
	if (cheri_gettag(args->dest_buf))
		printf("\t\tsuccess!\ncoproctest: (received \"%s\")\n", (*result_str));
	else
		err(errno, "coproctest: do_tests: corecv failed");
	pthread_mutex_unlock(&start);
	return (NULL);
}

static void 
do_tests(void)
{
	pollcoport_t pct;
	namespace_t *proc_ns;
	coport_t *port;
	nsobject_t *port_obj;
	int sent, recvd, error;
	char *recv;
	char *ns_name;
	pthread_t recvr;
 	corecv_thr_args_t *recvr_args;

 	pthread_mutex_init(&start, NULL);
 	pthread_cond_init(&started, NULL);
 	pthread_mutex_init(&procdeath, NULL);
 	pthread_cond_init(&proc_died, NULL);

 	recvr_args = calloc(1, sizeof(corecv_thr_args_t));
	ns_name = calloc(32, sizeof(char));
	strcpy(ns_name, default_name);
	

coproc_init_lbl:
	printf("coproctest: initializing coproc...");
	global_ns = coproc_init(NULL, NULL, NULL, NULL);
	if (global_ns != NULL) 
		printf("\t\tsuccess!\n");
	else {
		if (errno == EAGAIN) {
			printf("\t\tfailed. retrying...\n");
			sleep(1);
			goto coproc_init_lbl;
		}
		printf("\n");
		err(errno, "do_tests: coproc_init failed");
	}
	
	printf("coproctest: creating namespace...");
cocreate_lbl:
	proc_ns = cocreate(ns_name, APPLICATION, global_ns);
	if (proc_ns != NULL) 
		printf("\t\tsuccess!\n");
	else {
		if (errno == EEXIST && ns_name[10] < 'Z') {
			ns_name[10]++;
			goto cocreate_lbl;
		}
		err(errno, "coproctest: do_tests: cocreate failed");
	}
	
	printf("coproctest: creating named COCHANNEL...");
	port_obj = open_named_coport("test_coport", COCHANNEL, proc_ns);
	if (port_obj != NULL)
		printf("\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: open_named_coport failed");
	port = port_obj->coport;
	
	printf("coproctest: sending message...");
	sent = cosend(port_obj->coport, test_str, strlen(test_str));
	if (sent > 0)
		printf("\t\t\tsuccess!\ncoproctest: (sent \"%s\")\n", test_str);
	else
		err(errno, "coproctest: do_tests: cosend failed");
	
	printf("coproctest: receiving message...");
	recv = malloc(strlen(test_str)+1);
	recvd = corecv(port_obj->coport, (void **)&recv, strlen(test_str));
	if (recvd > 0)
		printf("\t\tsuccess!\ncoproctest: (received \"%s\")\n", recv);
	else
		err(errno, "coproctest: do_tests: corecv failed");
	free(recv);
	
	printf("coproctest: deleting ns object...");
	error = codelete(port_obj, proc_ns);
	if (error == 0)
		printf("\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: delete failed");
	port_obj = NULL;

	printf("coproctest: closing coport...");
	error = coclose(port);
	if (error == 0)
		printf("\t\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: coclose failed");
	port = NULL;

	printf("coproctest: creating reservation...");
	port_obj = coinsert("test_coport", RESERVATION, NULL, proc_ns);
	if (port_obj != NULL)
		printf("\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: coinsert failed");

	printf("coproctest: creating unnamed COPIPE...");
	port = open_coport(COPIPE);
	if (port != NULL)
		printf("\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: open_coport failed");
	
	printf("coproctest: updating RESERVATION->COPORT");
	port_obj = coupdate(port_obj, COPORT, port);
	if (port_obj != NULL)
		printf("\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: coupdate failed");
	
	printf("coproctest: sending message...");
	recv = malloc(strlen(test_str)+1);
	recvr_args->port = port;
	recvr_args->dest_buf = &recv;
	recvr_args->len = strlen(test_str);
	recvr_args->result_ptr = &recvd;
	recvd = 0;
	pthread_create(&recvr, NULL, do_copipe_recv, recvr_args);
	sent = cosend(port_obj->coport, test_str, strlen(test_str));
	if (sent > 0)
		printf("\t\t\tsuccess!\ncoproctest: (sent \"%s\")\n", test_str);
	else
		err(errno, "coproctest: do_tests: cosend failed");
	
	printf("coproctest: receiving message...");
	pthread_join(recvr, NULL);
	if (recvd > 0)
		printf("\t\tsuccess!\ncoproctest: (received \"%s\")\n", recv);
	else
		err(errno, "coproctest: do_tests: corecv failed");
	free(recv);
	recv = NULL;

	printf("coproctest: closing coport...");
	error = coclose(port);
	if (error == 0)
		printf("\t\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: coclose failed");

	printf("coproctest: deleting coport namespace object...");
	error = codelete(port_obj, proc_ns);
	if (error == 0)
		printf("\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: delete failed");

	printf("coproctest: creating unnamed COCARRIER...");
	port = open_coport(COCARRIER);
	if (port != NULL)
		printf("\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: open_coport failed");

	printf("coproctest: naming coport...");
	port_obj = coinsert("test_coport", COPORT, port, proc_ns);
	if (port_obj != NULL)
		printf("\t\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: coinsert failed");

	make_pollcoport(&pct, port, COPOLL_OUT);
	printf("coproctest: polling coport (is send possible?)...\n");
	error = copoll(&pct, 1, 0);
	if (error == -1) {
		printf("\t\t\tfailed\n");
		err(errno, "do_cocarrier: copoll failed");
	}
	else
		printf("coproctest: polling coport... \t\t\tsuccess!\n");


	recvr_args->port = port;
	recvd = 0;
	pthread_mutex_lock(&start);
	pthread_create(&recvr, NULL, do_cocarrier, recvr_args);
	pthread_cond_wait(&started, &start);
	pthread_mutex_unlock(&start);
	/* Still racey, but mildly less so. */
	pthread_yield();
	printf("coproctest: sending message...");
	sent = cosend(port_obj->coport, test_str, cheri_getlen(test_str));
	if (sent > 0)
		printf("\t\t\tsuccess!\ncoproctest: (sent \"%s\")\n", test_str);
	else
		err(errno, "coproctest: do_tests: cosend failed");
	
	pthread_join(recvr, NULL);
	
	printf("coproctest: unnaming coport...");
	error = codelete(port_obj, proc_ns);
	if (error == 0)
		printf("\t\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: codelete failed");

	printf("coproctest: closing coport...");
	error = coclose(port);
	if (error == 0)
		printf("\t\t\tsuccess!\n");
	else
		err(errno, "coproctest: do_tests: coclose failed");


}

int main(int argc, char *const argv[])
{
	int error;
	void *coproc_init_scb;
	pid_t test_pid, coprocd_pid;

    error = colookup(U_COPROC_INIT, &coproc_init_scb);
    if (error != 0) {
	    test_pid = getpid();
	    coprocd_pid = vfork();
	    if (coprocd_pid == 0)
	    	coexecve(test_pid, coprocd_args[0], coprocd_args, environ);
	    do {
	    	sleep(1);
	    	error = colookup(U_COPROC_INIT, &coproc_init_scb);
	    	sleep(1);
	    } while(error != 0);
	}
    set_ukern_target(COCALL_COPROC_INIT, coproc_init_scb);

    do_tests();
    do_procdeath_test();

	return (0);
}