PROG := ipcd

SRCS :=	coclose.c \
	coopen.c \
	copoll.c \
	copoll_deliver.c \
	copoll_utils.c \
	coport_table.c \
	coport_utils.c \
	corecv.c \
	cosend.c \
	ipcd.c \
	ipcd_cap.c \
	ipcd_startup.c 

DEP_LIBS := pthread comsg ccmalloc coproc cocall

include $(MK_DIR)/comesg.prog.mk

