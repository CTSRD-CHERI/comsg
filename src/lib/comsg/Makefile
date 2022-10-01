LIB := comsg

SRCS := ukern_calls.c \
	coport_ipc.c \
	coport_ipc_utils.c \
	coport_cinvoke.c \
	$(ARCH)/coport_cinvoke_stub.S \
	$(ARCH)/copipe_cinvoke_stubs.S \
	$(ARCH)/cochannel_cinvoke_stubs.S \
	namespace.c		\
	namespace_object.c		\
	coservice.c		\
	utils.c		

VPATH := $(CURDIR)/$(ARCH)
INC_FLAGS += -I$(CURDIR)/$(ARCH)

DEP_LIBS := pthread statcounters
DEP_ARS := cocall

SHLIB_MAJOR=	2

include $(MK_DIR)/comsg.lib.mk

