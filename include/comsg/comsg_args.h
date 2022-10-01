#ifndef _COMSG_ARGS_H
#define _COMSG_ARGS_H

#include <cocall/cocall_args.h>
#include <comsg/coevent.h>
#include <comsg/coport.h>
#include <comsg/coservice.h>
#include <comsg/namespace.h>
#include <comsg/namespace_object.h>

#pragma push_macro("UKERN_ENDPOINT")
#define UKERN_ENDPOINT(name)    COCALL_##name,
typedef enum cocall_ops {
    COCALL_INVALID = 0,
#include <comsg/ukern_calls.inc>
    N_UKERN_CALLS,
} cocall_num_t;
#pragma pop_macro("UKERN_ENDPOINT")

//There is an unsatisfying situation here which arises from the fact that
//cocall/coaccept data lengths must (sort of) be symmetrical, so to pass a variable 
//length array, even though we can derive its length, we must pass a pointer
//to memory we cannot guarantee will not be touched or unmapped during the call.
typedef struct _pollcoport_t
{
	coport_t *coport;
	coport_eventmask_t events;
	coport_eventmask_t revents;
} pollcoport_t;

struct comsg_args {
    int status;
    int error;
    cocall_num_t op;
    namespace_t *ns_cap;
    union 
    {
        struct {
            void *codiscover;
            void *coinsert;
            void *coselect;
            void *done_scb;
        }; //coproc_init, coproc_init_done
        struct {
            char ns_name[NS_NAME_LEN];
            nstype_t ns_type;
            namespace_t *child_ns_cap;
        }; //cocreate, codrop
        struct {
            char nsobj_name[NS_NAME_LEN];
            nsobject_t *nsobj;
            nsobject_type_t nsobj_type;
            union {
                void *obj;
                coservice_t *coservice;
                coport_t *coport;
            };
            void *scb_cap;
        }; //coupdate, coinsert, codelete, coselect, codiscover
        struct {
            void **worker_scbs;
            int nworkers;
            coservice_t *service;
            struct _coservice_endpoint *endpoint;
            coservice_flags_t service_flags;
            int target_op;
        }; //coprovide
        struct {
            coport_type_t coport_type;
            coport_t *port;
        }; //coopen, coclose
        struct {
            pollcoport_t *coports;
            uint ncoports;
            long timeout; 
        }; //copoll
        struct {
            coport_t *cocarrier;
            void *message;
            size_t length;
        }; //cosend/corecv
        struct {
            coevent_subject_t subject;
            coevent_t *coevent;
            cocallback_func_t *ccb_func;
            struct cocallback_args ccb_args;
            coevent_type_t event;
        }; //colisten, ccb_install
        struct {
            void *provider_scb;
            cocallback_flags_t flags;
        }; //ccb_register
    };
    char pad[64]; /* to match sizeof(struct cocall_args) */
};

typedef struct comsg_args comsg_args_t;
typedef struct comsg_args copoll_args_t;
typedef struct comsg_args cosend_args_t;
typedef struct comsg_args corecv_args_t;
typedef struct comsg_args codiscover_args_t;
typedef struct comsg_args coprovide_args_t;
typedef struct comsg_args coopen_args_t;
typedef struct comsg_args coclose_args_t;
typedef struct comsg_args coselect_args_t;
typedef struct comsg_args coinsert_args_t;
typedef struct comsg_args coproc_init_args_t;
typedef struct comsg_args codrop_args_t;
typedef struct comsg_args codelete_args_t;
typedef struct comsg_args coupdate_args_t;
typedef struct comsg_args cocreate_args_t;
typedef struct comsg_args colisten_args_t;
typedef struct comsg_args ccb_register_args_t;
typedef struct comsg_args ccb_install_args_t;

#endif //!defined(_COMSG_ARGS_H)