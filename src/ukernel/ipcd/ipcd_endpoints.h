#pragma once

#include <comsg/comsg_args.h>

#include "coclose.h"
#include "coopen.h"
#include "copoll.h"
#include "cosend.h"
#include "corecv.h"
#include "comsg_free.h"

#define COCALL_ENDPOINT_IMPL
#include <cocall/endpoint.h>
#undef COCALL_ENDPOINT_IMPL
