#ifndef COACCEPT_ENDPOINT
#error Must define COACCEPT_ENDPOINT(name, op, validate, func)
#endif 

#ifndef DECLARE_COACCEPT_ENDPOINT
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(#name, COCALL_##name, validate_f, operation_f)
#endif

DECLARE_COACCEPT_ENDPOINT(COOPEN, validate_coopen_args, coport_open)
DECLARE_COACCEPT_ENDPOINT(COCLOSE, validate_coclose_args, coport_close)
DECLARE_COACCEPT_ENDPOINT(COSEND, validate_cosend_args, coport_send)
DECLARE_COACCEPT_ENDPOINT(CORECV, validate_corecv_args, coport_recv)
DECLARE_COACCEPT_ENDPOINT(COPOLL, validate_copoll_args, cocarrier_poll)