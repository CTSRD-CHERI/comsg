PROG := coprocd

SRCS :=	coprocd.c \
	daemon.c \
	launch.c \
	module.c \
	modules.c \
	monitor.c \
	runloop.c \
	util.c \
	dynamic_endpoint.c \
	dynamic_endpoint_map.c 

SRCS += $(wildcard modules/*.c)

DEP_LIBS := pthread comsg cocall


include $(MK_DIR)/comsg.prog.mk

