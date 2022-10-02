#ifndef COACCEPT_ENDPOINT
#error Must define COACCEPT_ENDPOINT(name, op, validate, func)
#endif 

#ifndef DECLARE_COACCEPT_ENDPOINT
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(#name, COCALL_##name, validate_f, operation_f)
#endif

DECLARE_COACCEPT_ENDPOINT(CCB_INSTALL, validate_cocallback_install, install_cocallback)
DECLARE_COACCEPT_ENDPOINT(CCB_REGISTER, validate_cocallback_register, cocallback_register)
DECLARE_COACCEPT_ENDPOINT(COLISTEN, validate_colisten, add_event_listener)
