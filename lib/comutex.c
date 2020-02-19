#include "comutex.h"
#include "coproc.h"

#include <stdatomic.h>

int counlock(comutex_t * mtx)
{
	void * __capability sw_code, sw_data, func;
	int error;

	error=ukern_lookup(&sw_code,&sw_data,U_COUNLOCK,&func);
}

int colock(comutex_t * mtx)
{
	void * __capability sw_code, sw_data, func;
	int error;

	error=ukern_lookup(&sw_code,&sw_data,U_COLOCK,&func);

}

int comutex_init(comutex_t * mtx);
{
	void * __capability sw_code, sw_data, func;
	int error;

	error=ukern_lookup(&sw_code,&sw_data,U_COMUTEX_INIT,&func);

}