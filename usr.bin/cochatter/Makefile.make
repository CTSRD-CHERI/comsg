.include <src.opts.mk>

PROG=	cochatter
SRCS=	comsg_chatterer.c comutex.c comsg.c coproc.c 

CFLAGS+=	-fPIE -pie -Wno-error=deprecated-declarations -fuse-ld=lld
NEED_CHERI=	pure
MK_CHERI_SHARED_PROG:=yes

.include <bsd.prog.mk>