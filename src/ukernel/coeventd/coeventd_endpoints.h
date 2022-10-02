#pragma once

#include <comsg/comsg_args.h>

#include "cocallback_install.h"
#include "cocallback_register.h"
#include "coevent_listen.h"

#define COCALL_ENDPOINT_IMPL
#include <cocall/endpoint.h>
#undef COCALL_ENDPOINT_IMPL
