CC=cheribsd128purecap-clang
LIB=pthread
INC=include/ src/ukernel/include 

LIB_PARAMS=$(foreach l, $(LIB),	 -l$l)
INC_PARAMS=$(foreach i, $(INC), -I$i)

CFLAGS=-v -ggdb
BUILDDIR=build
OUTDIR=output

default : ukernel

ukernel : comesg_kern.o coport_utils.o comsg.o sys_comutex.o comutex.o coproc.o
	$(CC) $(CCFLAGS) $(LIB_PARAMS) $(INC_PARAMS)  -o $(OUTDIR)/comesg_ukernel $(foreach o, $^, $(BUILDDIR)/$o)


comesg_kern.o : src/ukernel/comesg_kern.c \
	src/ukernel/include/comesg_kern.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/sys_comutex.h include/coport_utils.h \
	include/coproc.h include/comsg.h
	$(CC) $(CCFLAGS) $(INC_PARAMS) -c src/ukernel/comesg_kern.c -o $(BUILDDIR)/comesg_kern.o


comutex.o: src/lib/comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h include/coproc.h
	$(CC) $(CCFLAGS) $(INC_PARAMS) -c src/lib/comutex.c -o $(BUILDDIR)/comutex.o


sys_comutex.o: src/ukernel/sys_comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h src/ukernel/include/sys_comutex.h
	$(CC) $(CCFLAGS) $(INC_PARAMS) -c src/ukernel/sys_comutex.c -o $(BUILDDIR)/sys_comutex.o

comsg.o: src/lib/comsg.c \
	include/coproc.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h include/comsg.h
	$(CC) $(CCFLAGS) $(INC_PARAMS) -c src/lib/comsg.c -o $(BUILDDIR)/comsg.o


coproc.o: src/lib/coproc.c include/coproc.h
	$(CC) $(CCFLAGS) $(INC_PARAMS) -c src/lib/coproc.c -o $(BUILDDIR)/coproc.o


coport_utils.o: src/lib/coport_utils.c include/coport_utils.h \
	include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/comesg_kern.h src/ukernel/include/sys_comutex.h
	$(CC) $(CCFLAGS) $(INC_PARAMS) -c src/lib/coport_utils.c -o $(BUILDDIR)/coport_utils.o
