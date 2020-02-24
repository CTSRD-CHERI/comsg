.include <src.opts.mk>

PRO =	comesg_ukernel
SRCS=	comesg_kern.c comutex.c sys_comutex.c coport_utils.c 
NEED_CHERI=	pure
MK_CHERI_SHARED_PROG:=yes

WARNS?= 5

.include <bsd.prog.mk>

