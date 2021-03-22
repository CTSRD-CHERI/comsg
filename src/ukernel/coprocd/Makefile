PROG := coprocd

SRCS :=	coprocd.c \
	daemon.c \
	launch.c \
	module.c \
	modules.c \
	monitor.c \
	runloop.c \
	util.c

SRCS += $(wildcard modules/*.c)

DEP_LIBS := pthread comsg ccmalloc coproc cocall


include $(MK_DIR)/comsg.prog.mk

