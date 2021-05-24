PROG := coeventd

SRCS :=	coeventd.c \
	cocallback_func_utils.c \
	cocallback_install.c \
	cocallback_register.c \
	coevent_listen.c \
	coevent_utils.c \
	coeventd_setup.c \
	procdeath.c \
	procdeath_tbl.c

DEP_LIBS := pthread comsg ccmalloc coproc cocall

include $(MK_DIR)/comsg.prog.mk

