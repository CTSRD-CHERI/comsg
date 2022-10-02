#ifndef SLOACCEPT_ENDPOINT
#error Must define SLOACCEPT_ENDPOINT(name, op, validate, func)
#endif 

#ifndef DECLARE_SLOACCEPT_ENDPOINT
#define DECLARE_SLOACCEPT_ENDPOINT(name, validate_f, operation_f) SLOACCEPT_ENDPOINT(#name, COCALL_##name, validate_f, operation_f)
#endif

DECLARE_SLOACCEPT_ENDPOINT(SLOPOLL, validate_copoll_args, cocarrier_poll_slow)