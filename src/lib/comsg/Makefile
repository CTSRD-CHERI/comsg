LIB := comsg

SRCS := ukern_calls.c \
	coport_ipc.c \
	coport_ipc_utils.c \
	coport_cinvoke.c \
	coport_cinvoke_stub.S

VPATH := $(CURDIR)/$(ARCH)
INC_FLAGS += -I$(CURDIR)/$(ARCH)

DEP_LIBS := pthread cocall coproc

SHLIB_MAJOR=	2

include $(MK_DIR)/comsg.lib.mk

