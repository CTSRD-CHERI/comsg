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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

extern char **environ;

int main(int argc, char const *argv[])
{
	pid_t memcpy_pid;
	setenv("LD_BIND_NOW","yesplease",1);
	char *memcpy_args[5];


	memcpy_args[0] = strdup("/usr/bin/memcpy_bmark");
	memcpy_args[1] = strdup("-r24");
	memcpy_args[3] = strdup("-q");
	memcpy_args[4] = NULL;

	//int opt;
	//char * strptr;
	/*
	int receive = 0;
	
	int explicit = 0;
	while((opt=getopt(argc,argv,"ot:r:b:pqc:"))!=-1)
	{
		switch(opt)
		{
			case 'r':
				runs = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || runs <= 0)
					err(1,"invalid runs");
				break;
			case 't':
				total_size = strtol(optarg, &strptr, 10);
				if (*optarg == '\0' || *strptr != '\0' || total_size <=0)
					err(1,"invalid total length");
				break;
			case 'p':
				receive=1;
				break;
			case 'c':
				receive=1;
				break;
			default:
				break;
		
	return 0;*/
	char * buf_size_str = malloc(10*sizeof(char));
	int memcpy_status;
	for(size_t i = 1; i<=1048576UL; i*=10)
	{
		memset(buf_size_str,0,10);
		sprintf(buf_size_str,"-b%lu",i);

		memcpy_args[2] = strdup(buf_size_str);
		if(!(memcpy_pid=fork()))
		{
			//child
			if(execve(memcpy_args[0],memcpy_args,environ))
				err(errno,"main:execve failed to execute memcpy_bmark");
		}	
		waitpid(memcpy_pid,&memcpy_status,WEXITED);
		printf("Done %lu bytes.\n",i);
		
	}
	
	return (0);
}