#ifndef COACCEPT_ENDPOINT
#error Must define COACCEPT_ENDPOINT(name, op, validate, func)
#endif 

#ifndef DECLARE_COACCEPT_ENDPOINT
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(#name, COCALL_##name, validate_f, operation_f)
#endif

DECLARE_COACCEPT_ENDPOINT(CODISCOVER, validate_codiscover_args, discover_coservice)
DECLARE_COACCEPT_ENDPOINT(COPROVIDE, validate_coprovide_args, provide_coservice)
DECLARE_COACCEPT_ENDPOINT(COPROVIDE2, validate_coprovide2_args, provide_coservice2)
