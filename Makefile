LIB=pthread
INC=include/ src/ukernel/include 

BUILDDIR=build
OUTDIR=output

LIB_PARAMS=-L$(CURDIR)/$(OUTDIR) $(foreach l, $(LIB),	 -l$l) 
INC_PARAMS=$(foreach i, $(INC), -I$i)

BUILD_TIME:=$(shell date "+%Y/%m/%d %H:%M:%S")
DEBUG=-v -g
CFLAGS=$(DEBUG) -integrated-as -G0 -msoft-float -cheri=128 -mcpu=cheri128 \
	-mabi=purecap -fPIE -mstack-alignment=16 -fPIC

LLDFLAGS=-pie -fuse-ld=lld 
ifndef FORCE
FORCE=--force --clean
endif
ifdef CHERI_ROOT
CHERI_FSDIR=$(CHERI_ROOT)/extra-files
CHERIBSD_DIR=$(CHERI_ROOT)/cheribsd
CHERIBUILD_DIR=$(CHERI_ROOT)/cheribuild/
endif

default : ukernel cochatter libcomsg.so
	#cp $(OUTDIR)/comesg_ukernel $(CHERI_FSDIR)/root/bin
	#cp $(OUTDIR)/cochatter $(CHERI_FSDIR)/root/bin
	#cp $(OUTDIR)/libcomsg.so $(C)
	$(CHERIBUILD_DIR)/cheribuild.py --skip-update $(FORCE)  \
	cheribsd-mips-purecap disk-image-mips-purecap

run : ukernel cochatter
	cp $(OUTDIR)/comesg_ukernel $(CHERI_FSDIR)/root/bin
	cp $(OUTDIR)/cochatter $(CHERI_FSDIR)/root/bin
ifdef dev
	git commit -a --message="$(BUILD_TIME)"
endif
	$(CHERIBUILD_DIR)/cheribuild.py --skip-update -v $(FORCE) \
	--cheribsd-purecap/subdir="'usr.bin/comesg_ukernel' 'usr.bin/cochatter'" \
	--cheribsd/subdir="'usr.bin/comesg_ukernel' 'usr.bin/cochatter'" \
	cheribsd-purecap disk-image-purecap run-purecap \
	cheribsd-mips-purecap disk-image-mips-purecap run-mips-purecap

ukernel : comesg_kern.o coport_utils.o sys_comutex.o comutex.o \
	ukern_mman.o ukern_commap.o libcomsg.so
	$(CC) $(CFLAGS) $(LLDFLAGS) -Wl,-znow $(LIB_PARAMS) -lcomsg $(INC_PARAMS)  \
	-o $(OUTDIR)/comesg_ukernel $(BUILDDIR)/comesg_kern.o \
	$(BUILDDIR)/coport_utils.o $(BUILDDIR)/sys_comutex.o \
	$(BUILDDIR)/comutex.o $(BUILDDIR)/ukern_mman.o $(BUILDDIR)/ukern_commap.o
	$(CHERIBUILD_DIR)/cheribuild.py --skip-update $(FORCE)  \
	--cheribsd-mips-purecap/subdir="'usr.bin/comesg_ukernel'" \
	--cheribsd/subdir="'usr.bin/comesg_ukernel'" \
	cheribsd-mips-purecap disk-image-mips-purecap

cochatter : comsg_chatterer.o libcomsg.so
	$(CC) $(CFLAGS)  $(LLDFLAGS) $(LIB_PARAMS) -lcomsg $(INC_PARAMS) \
	-o $(OUTDIR)/cochatter $(BUILDDIR)/comsg_chatterer.o
	$(CHERIBUILD_DIR)/cheribuild.py --skip-update $(FORCE)  \
	--cheribsd-mips-purecap/subdir="'usr.bin/cochatter'" \
	--cheribsd/subdir="'usr.bin/cochatter'" \
	cheribsd-mips-purecap disk-image-mips-purecap

comsg_chatterer.o : src/bin/comsg_chatterer.c include/comsg.h \
	src/ukernel/include/sys_comsg.h include/coport.h \
	include/comutex.h include/coproc.h include/commap.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/bin/comsg_chatterer.c \
	-o $(BUILDDIR)/comsg_chatterer.o
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/cochatter;)

comesg_kern.o : src/ukernel/comesg_kern.c \
	src/ukernel/include/ukern_params.h \
	src/ukernel/include/comesg_kern.h include/coport.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/sys_comutex.h include/coport_utils.h \
	include/coproc.h include/comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/comesg_kern.c \
	-o $(BUILDDIR)/comesg_kern.o
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/comesg_ukernel;)

ukern_mman.o : src/ukernel/ukern_mman.c \
	src/ukernel/include/ukern_mman.h src/ukernel/include/ukern_params.h \
	src/ukernel/include/sys_comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/ukern_mman.c \
	-o $(BUILDDIR)/ukern_mman.o
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/comesg_ukernel;)

ukern_commap.o : src/ukernel/ukern_commap.c \
	src/ukernel/include/ukern_params.h src/ukernel/include/comesg_kern.h  \
	src/ukernel/include/sys_comsg.h include/commap.h include/coproc.h \
	src/ukernel/include/ukern_commap.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/ukern_commap.c \
	-o $(BUILDDIR)/ukern_commap.o
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/comesg_ukernel;)


comutex.o: src/lib/comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h include/coproc.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/comutex.c -o $(BUILDDIR)/comutex.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
	cp $< $(CHERIBSD_DIR)/usr.bin/cochatter
endif

sys_comutex.o: src/ukernel/sys_comutex.c include/comutex.h \
	src/ukernel/include/sys_comsg.h src/ukernel/include/sys_comutex.h
	$(CC) $(CFLAGS)  $(INC_PARAMS) -c src/ukernel/sys_comutex.c \
	-o $(BUILDDIR)/sys_comutex.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
	cp $< $(CHERIBSD_DIR)/usr.bin/cochatter
endif

libcomsg.so: src/lib/libcomsg.c src/lib/coproc.c \
	include/coproc.h include/coport.h  include/comutex.h\
	src/ukernel/include/sys_comsg.h include/comsg.h \
	include/commap.h src/lib/commap.c
	$(CC) $(CFLAGS) $(INC_PARAMS) -shared -o $(OUTDIR)/libcomsg.so src/lib/libcomsg.c src/lib/commap.c src/lib/coproc.c
ifdef CHERIBSD_DIR
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/lib/libcomsg;)
	rm -f $(CHERIBSD_DIR)/lib/libcomsg/*.o
endif

commap.o: src/lib/commap.c include/commap.h \
	src/ukernel/include/sys_comsg.h include/coproc.h src/lib/coproc.c
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/commap.c -o $(BUILDDIR)/commap.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
	cp $< $(CHERIBSD_DIR)/usr.bin/cochatter
endif

coproc.o: src/lib/coproc.c include/coproc.h include/commap.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coproc.c -o $(BUILDDIR)/coproc.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
	cp $< $(CHERIBSD_DIR)/usr.bin/cochatter
endif

coport_utils.o: src/lib/coport_utils.c include/coport_utils.h \
	include/coport.h src/ukernel/include/ukern_mman.h \
	include/comutex.h src/ukernel/include/sys_comsg.h \
	src/ukernel/include/comesg_kern.h src/ukernel/include/sys_comutex.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coport_utils.c \
	-o $(BUILDDIR)/coport_utils.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
	cp $< $(CHERIBSD_DIR)/usr.bin/cochatter
endif
