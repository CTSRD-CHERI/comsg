LIB := comsg

SRCS := ukern_calls.c \
	coport_ipc.c \
	coport_ipc_utils.c \
	coport_cinvoke.c \
	$(ARCH)/coport_cinvoke_stub.S \
	namespace.c		\
	namespace_object.c		\
	coservice.c		\
	utils.c		

ifeq ($(ARCH),riscv)
SRCS += $(ARCH)/copipe_cinvoke_stubs.S \
	$(ARCH)/cochannel_cinvoke_stubs.S 
else ifeq ($(ARCH),arm)
SRCS += $(ARCH)/coport_cinvoke_stubs.c
endif

VPATH := $(CURDIR)/$(ARCH)
INC_FLAGS += -I$(CURDIR)/$(ARCH)
CFLAGS += -DENABLE_INTERNAL_COMSG_BENCHMARK

DEP_LIBS := pthread
ifeq ($(ARCH),riscv)
DEP_LIBS += statcounters
endif
DEP_ARS := cocall

SHLIB_MAJOR=	2

include $(MK_DIR)/comsg.lib.mk

