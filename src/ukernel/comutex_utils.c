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
	This file contains functions for comutexes, a feature I initially conceived
	to allow cross-process synchronisation. I found that it was not essential
	for the MPhil as I focused in more and more on the IPC model, but could be
	useful if developed further and might solve some stability problems if
	sufficiently well implemented. -PBB
*/

//NOTE-PBB: Many of these functions, if used, would be better situated in the
//relevant ukern_* files for the subsystems they pertain to.


typedef struct _comutex_tbl_entry_t
{
    unsigned int id;
    sys_comutex_t mtx;
} comutex_tbl_entry_t;

typedef struct _comutex_tbl_t
{
    int index;
    pthread_mutex_t lock;
    comutex_tbl_entry_t * table;
} comutex_tbl_t;


typedef struct _comutex_init_args_t
{
    char name[COMUTEX_NAME_LEN];
} cocall_comutex_init_args_t;

typedef struct _cocall_comutex_init_t
{
    cocall_comutex_init_args_t args;
    _Atomic(comutex_t * __capability) mutex; 
} __attribute__((__aligned__(16))) cocall_comutex_init_t;

typedef struct _colock_args_t
{
    _Atomic(comutex_t * __capability) mutex;
    int result;
} __attribute__((__aligned__(16))) colock_args_t;
typedef struct _colock_args_t counlock_args_t;

const int COMTX_TBL_LEN = (MAX_COMUTEXES*sizeof(comutex_tbl_entry_t));


int comutex_tbl_setup(void)
{
    pthread_mutexattr_t lock_attr;
    pthread_mutexattr_init(&lock_attr);
    pthread_mutexattr_settype(&lock_attr,PTHREAD_MUTEX_RECURSIVE);
    int error=pthread_mutex_init(&comutex_table.lock,&lock_attr);
    comutex_table.index=0;
    comutex_table.table=ukern_malloc(COMTX_TBL_LEN);
    mlock(comutex_table.table,COMTX_TBL_LEN);
    /* reserve a superpage or two for this, entries should be small */
    /* reserve a few superpages for ports */
    return error;
}


int comutex_deinit(comutex_tbl_entry_t * m)
{
    sys_comutex_t mtx = m->mtx;
    free(mtx.kern_mtx->lock);
    free(mtx.kern_mtx);
    mtx.user_mtx=NULL;

    // remove from table?
    return 0;
}

void create_comutex(comutex_t * cmtx,char * name)
{
    int error;
    sys_comutex_t * sys_mtx;
    comutex_tbl_entry_t table_entry;

    sys_mtx=ukern_malloc(sizeof(sys_comutex_t));
    error=sys_comutex_init(name,sys_mtx);
    
    table_entry.id=generate_id();
    table_entry.mtx=*sys_mtx;

    cmtx->mtx=sys_mtx->user_mtx;
    strcpy(cmtx->name,name);

    error=add_mutex(table_entry);

    return;
}

void *comutex_setup(void *args)
{
    worker_args_t * data=args;
    cocall_comutex_init_t comutex_args;
    comutex_tbl_entry_t table_entry;
    sys_comutex_t * mtx;

    int error;
    int index;
    int lookup;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;
    

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COMUTEX_INIT);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,&comutex_args,sizeof(comutex_args));
        /* check args are acceptable */

        /* check if mutex exists */
        pthread_mutex_lock(&comutex_table.lock);
        lookup=lookup_mutex(comutex_args.args.name,&mtx);
        if(lookup==1)
        {
            /* if it doesn't, set up mutex */
            mtx=ukern_malloc(sizeof(sys_comutex_t));
            error=sys_comutex_init(comutex_args.args.name,mtx);
            table_entry.mtx=*mtx;
            table_entry.id=generate_id();
            index=add_mutex(table_entry);
            if(error!=0)
            {
                err(1,"unable to init_port");
            }
        }
        pthread_mutex_unlock(&comutex_table.lock);
        strcpy(comutex_args.mutex->name,mtx->name);
        comutex_args.mutex->mtx=mtx->user_mtx;
        comutex_args.mutex->key=NULL;
        
    }
    return 0;
}

void *comutex_lock(void *args)
{
    worker_args_t * data=args;
    colock_args_t colock_args;
    sys_comutex_t * mtx;
    comutex_t * user_mutex;

    int error;
    int lookup;

    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability caller_cookie;
    void * __capability target;

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COLOCK);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,&colock_args,sizeof(colock_args));
        /* check args are acceptable */
        // validation
        /* check if mutex exists */
        lookup=lookup_mutex(colock_args.mutex->name,&mtx);
        if(lookup==0)
        {
            user_mutex=colock_args.mutex;
            error=sys_colock(mtx,user_mutex->key);
            mtx->key=user_mutex->key;
        }
        //report errors
    }
    return 0;
}

void *comutex_unlock(void *args)
{
    void * __capability sw_code;
    void * __capability sw_data;
    void * __capability  caller_cookie;
    void * __capability  target;

    worker_args_t * data=args;
    counlock_args_t colock_args;
    sys_comutex_t * mtx;
    comutex_t * user_mutex;

    int error;
    int lookup;

    error=coaccept_init(&sw_code,&sw_data,data->name,&target);
    data->cap=target;
    update_worker_args(data,U_COUNLOCK);
    for (;;)
    {
        error=coaccept(sw_code,sw_data,&caller_cookie,&colock_args,sizeof(colock_args));
        /* check args are acceptable */
        // validation
        /* check if mutex exists */
        lookup=lookup_mutex(colock_args.mutex->name,&mtx);
        if(lookup==0)
        {
            user_mutex=colock_args.mutex;
            error=sys_counlock(mtx,user_mutex->key);
            mtx->key=user_mutex->key;
        }
        //report errors
    }
    return 0;
}

int add_mutex(comutex_tbl_entry_t entry)
{
    int entry_index;
    
    pthread_mutex_lock(&comutex_table.lock);
    if(comutex_table.index>=MAX_COPORTS)
    {
        pthread_mutex_unlock(&comutex_table.lock);
        return 1;
    }
    comutex_table.table[comutex_table.index]=entry;
    entry_index=comutex_table.index++;
    pthread_mutex_unlock(&comutex_table.lock);
    return entry_index;
}

int lookup_mutex(char * mtx_name,sys_comutex_t ** mtx_buf)
{
    if (strlen(mtx_name)>COPORT_NAME_LEN)
    {
        err(1,"mtx name length too long");
    }
    for(int i = 0; i<comutex_table.index;i++)
    {
        if (comutex_table.table[i].mtx.user_mtx==NULL)
        {
            *mtx_buf=NULL;
            return 1;
        }
        if(strcmp(mtx_name,comutex_table.table[i].mtx.name)==0)
        {
            *mtx_buf=&comutex_table.table[i].mtx;
            return 0;
        }
    }
    *mtx_buf=NULL;
    return 1;
}