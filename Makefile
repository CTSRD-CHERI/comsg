CC=cheribsd128purecap-clang
LIB=pthread
INC=include/ src/ukernel/include 

LIB_PARAMS=$(foreach l, $(LIB),	 -l$l)
INC_PARAMS=$(foreach i, $(INC), -I$i)


DEBUG=-v -ggdb 
CFLAGS=$(DEBUG) -target mips64-unknown-freebsd13 -integrated-as -G0 -msoft-float -cheri=128 -mcpu=cheri128 --sysroot=/Users/peter/Projects/CHERI/output/sdk/sysroot128 -B/Users/peter/Projects/CHERI/output/sdk/bin -mabi=purecap
BUILDDIR=build
OUTDIR=output

CHERI_FSDIR=/Users/peter/Projects/cheri/extra-files/root/bin

default : ukernel cochatter
	cp $(OUTDIR)/comesg_ukernel $(CHERI_FSDIR)
	cp $(OUTDIR)/cochatter $(CHERI_FSDIR)

ukernel : comesg_kern.o coport_utils.o comsg.o sys_comutex.o comutex.o coproc.o
	$(CC) $(CFLAGS) $(LIB_PARAMS) $(INC_PARAMS)  -o $(OUTDIR)/comesg_ukernel $(foreach o, $^, $(BUILDDIR)/$o)

cochatter : comsg_chatterer.o comsg.o coproc.o comutex.o
	$(CC) $(CFLAGS) $(LIB_PARAMS) $(INC_PARAMS)  -o $(OUTDIR)/cochatter $(foreach o, $^, $(BUILDDIR)/$o)

comsg_chatterer.o : src/bin/comsg_chatterer.c include/comsg.h \
	src/ukernel/include/sys_comsg.h include/coport.h \
	include/comutex.h include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/bin/comsg_chatterer.c -o $(BUILDDIR)/comsg_chatterer.o

comesg_kern.o : src/ukernel/comesg_kern.c \
	src/ukernel/include/comesg_kern.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/sys_comutex.h include/coport_utils.h \
	include/coproc.h include/comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/comesg_kern.c -o $(BUILDDIR)/comesg_kern.o

comutex.o: src/lib/comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/comutex.c -o $(BUILDDIR)/comutex.o

sys_comutex.o: src/ukernel/sys_comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h src/ukernel/include/sys_comutex.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/sys_comutex.c -o $(BUILDDIR)/sys_comutex.o

comsg.o: src/lib/comsg.c \
	include/coproc.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h include/comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/comsg.c -o $(BUILDDIR)/comsg.o

coproc.o: src/lib/coproc.c include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coproc.c -o $(BUILDDIR)/coproc.o

coport_utils.o: src/lib/coport_utils.c include/coport_utils.h \
	include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/comesg_kern.h src/ukernel/include/sys_comutex.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coport_utils.c -o $(BUILDDIR)/coport_utils.o
