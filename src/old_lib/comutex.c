#include <unistd.h>
#include <string.h>
#include <stdatomic.h>

#include "comutex.h"
#include "coproc.h"



int counlock(comutex_t * mtx)
{
	counlock_args_t call_data;
	void * sw_code;
	void * sw_data;
	void * func;
	int error;

	error=ukern_lookup(&sw_code,&sw_data,U_COUNLOCK,&func);
	call_data.mutex=mtx;
	error=cocall(sw_code,sw_data,func,&call_data,sizeof(call_data));

	return call_data.result;
}

int colock(comutex_t * mtx, _Atomic(void * __capability) key)
{
	colock_args_t call_data;
	void * sw_code;
	void * sw_data;
	void * func;
	int error;


	error=ukern_lookup(&sw_code,&sw_data,U_COLOCK,&func);
	mtx->key=key;
	call_data.mutex=mtx;
	error=cocall(sw_code,sw_data,func,&call_data,sizeof(call_data));

	return call_data.result;
}

int comutex_init(char * mtx_name, comutex_t * mutex)
{
	/* call into ukernel to create shared place mtx->lock can live */
	void * sw_code;
	void * sw_data;
	void * func;
	cocall_comutex_init_t call_data;
	int error;

	error=ukern_lookup(&sw_code,&sw_data,U_COMUTEX_INIT,&func);

	strcpy(call_data.args.name,mtx_name);
	error=cocall(sw_code,sw_data,func,&call_data,sizeof(call_data));
	mutex=call_data.mutex;

	return 0;
}