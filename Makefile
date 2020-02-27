CC=cheribsd128purecap-clang
LIB=pthread
INC=include/ src/ukernel/include 

LIB_PARAMS=$(foreach l, $(LIB),	 -l$l)
INC_PARAMS=$(foreach i, $(INC), -I$i)




DEBUG=-v -ggdb 
CFLAGS=$(DEBUG) --target=mips64-unknown-freebsd13 -integrated-as -G0 \
	-msoft-float -cheri=128 -mcpu=cheri128 \
	--sysroot=/Users/peter/Projects/CHERI/output/sdk/sysroot128 \
	-B/Users/peter/Projects/CHERI/output/rootfs-purecap128 -mabi=purecap -fPIE \
	-mstack-alignment=16 -fpic
BUILDDIR=build
OUTDIR=output
LLDFLAGS=-pie -fuse-ld=lld

CHERI_FSDIR=/Users/peter/Projects/cheri/extra-files/root/bin

default : ukernel cochatter
	cp $(OUTDIR)/comesg_ukernel $(CHERI_FSDIR)
	cp $(OUTDIR)/cochatter $(CHERI_FSDIR)
	/Users/peter/Projects/CHERI/cheribuild/cheribuild.py --skip-update --force \
	--cheribsd-purecap/subdir usr.bin --cheribsd/subdir usr.bin \
	cheribsd cheribsd-purecap disk-image-purecap

run : ukernel cochatter
	cp $(OUTDIR)/comesg_ukernel $(CHERI_FSDIR)
	cp $(OUTDIR)/cochatter $(CHERI_FSDIR)
	/Users/peter/Projects/CHERI/cheribuild/cheribuild.py --skip-update --force \
	--cheribsd-purecap/subdir="'usr.bin/comesg_ukernel' 'usr.bin/cochatter'" \
	--cheribsd/subdir="'usr.bin/comesg_ukernel' 'usr.bin/cochatter'" \
	cheribsd cheribsd-purecap disk-image-purecap run-purecap

ukernel : comesg_kern.o coport_utils.o sys_comutex.o comutex.o coproc.o
	$(CC) $(CFLAGS) $(LLDFLAGS) $(LIB_PARAMS) $(INC_PARAMS)  \
	-o $(OUTDIR)/comesg_ukernel $(foreach o, $^, $(BUILDDIR)/$o)


cochatter : comsg_chatterer.o comsg.o coproc.o comutex.o
	$(CC) $(CFLAGS) $(LLDFLAGS) $(LIB_PARAMS) $(INC_PARAMS) \
	-o $(OUTDIR)/cochatter $(foreach o, $^, $(BUILDDIR)/$o)

comsg_chatterer.o : src/bin/comsg_chatterer.c include/comsg.h \
	src/ukernel/include/sys_comsg.h include/coport.h \
	include/comutex.h include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/bin/comsg_chatterer.c \
	-o $(BUILDDIR)/comsg_chatterer.o
	$(foreach c, $^, cp $c /Users/peter/Projects/CHERI/cheribsd/usr.bin/cochatter;)

comesg_kern.o : src/ukernel/comesg_kern.c \
	src/ukernel/include/comesg_kern.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/sys_comutex.h include/coport_utils.h \
	include/coproc.h include/comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/comesg_kern.c \
	-o $(BUILDDIR)/comesg_kern.o
	$(foreach c, $^, cp $c /Users/peter/Projects/CHERI/cheribsd/usr.bin/comesg_ukernel;)



comutex.o: src/lib/comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/comutex.c -o $(BUILDDIR)/comutex.o
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/comesg_ukernel
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/cochatter

sys_comutex.o: src/ukernel/sys_comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h src/ukernel/include/sys_comutex.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/sys_comutex.c \
	-o $(BUILDDIR)/sys_comutex.o
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/comesg_ukernel
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/cochatter



comsg.o: src/lib/comsg.c \
	include/coproc.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h include/comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/comsg.c -o $(BUILDDIR)/comsg.o
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/comesg_ukernel
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/cochatter

coproc.o: src/lib/coproc.c include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coproc.c -o $(BUILDDIR)/coproc.o
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/comesg_ukernel
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/cochatter

coport_utils.o: src/lib/coport_utils.c include/coport_utils.h \
	include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/comesg_kern.h src/ukernel/include/sys_comutex.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coport_utils.c \
	-o $(BUILDDIR)/coport_utils.o
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/comesg_ukernel
	cp $< /Users/peter/Projects/CHERI/cheribsd/usr.bin/cochatter

