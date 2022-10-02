#ifndef COACCEPT_ENDPOINT
#error Must define COACCEPT_ENDPOINT(name, op, validate, func)
#endif 

#ifndef DECLARE_COACCEPT_ENDPOINT
#define DECLARE_COACCEPT_ENDPOINT(name, validate_f, operation_f) COACCEPT_ENDPOINT(#name, COCALL_##name, validate_f, operation_f)
#endif

DECLARE_COACCEPT_ENDPOINT(COSELECT, validate_coselect_args, namespace_object_select)
DECLARE_COACCEPT_ENDPOINT(COINSERT, validate_coinsert_args, namespace_object_insert)
DECLARE_COACCEPT_ENDPOINT(COUPDATE, validate_coupdate_args, namespace_object_update)
DECLARE_COACCEPT_ENDPOINT(CODELETE, validate_codelete_args, namespace_object_delete)
DECLARE_COACCEPT_ENDPOINT(COCREATE, validate_cocreate_args, namespace_create)
DECLARE_COACCEPT_ENDPOINT(CODROP, validate_codrop_args, namespace_drop)
