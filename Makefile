LIB=pthread statcounters
INC=include/ src/ukernel/include 

BUILDDIR=build
OUTDIR=output

LIB_PARAMS=-L$(CURDIR)/$(OUTDIR) $(foreach l, $(LIB),	 -l$l) 
INC_PARAMS=$(foreach i, $(INC), -I$i)

BUILD_TIME:=$(shell date "+%Y/%m/%d %H:%M:%S")
DEBUG=-v -g
CFLAGS=$(DEBUG) -integrated-as -G0 -msoft-float -cheri=128 -mcpu=cheri128 \
	-mabi=purecap -fPIE -mstack-alignment=16 -fPIC

LLDFLAGS=-pie -fuse-ld=lld -Wl,-znow
ifndef FORCE
FORCE=--force --clean
endif
ifdef CHERI_ROOT
CHERI_FSDIR=$(CHERI_ROOT)/extra-files
CHERIBSD_DIR=$(CHERI_ROOT)/cheribsd
CHERIBUILD_DIR=$(CHERI_ROOT)/cheribuild/
endif

INCDIR=include
UKRN_SRCDIR=src/ukernel
UKRN_INCDIR=$(UKRN_SRCDIR)/include

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
	ukern_mman.o ukern_commap.o libcomsg.so ukern_tables.o ukern_requests.o \
	ukern_utils.o
	$(CC) $(CFLAGS) $(LLDFLAGS) $(LIB_PARAMS) -lcomsg $(INC_PARAMS)  \
	-o $(OUTDIR)/comesg_ukernel $(BUILDDIR)/comesg_kern.o \
	$(BUILDDIR)/coport_utils.o $(BUILDDIR)/sys_comutex.o \
	$(BUILDDIR)/comutex.o $(BUILDDIR)/ukern_mman.o $(BUILDDIR)/ukern_commap.o \
	$(BUILDDIR)/utils.o $(BUILDDIR)/ukern_tables.o $(BUILDDIR)/ukern_requests.o
	$(CHERIBUILD_DIR)/cheribuild.py --skip-update $(FORCE) \
	--cheribsd-mips-purecap/subdir="'usr.bin/comesg_ukernel'" \
	--cheribsd/subdir="'usr.bin/comesg_ukernel'" \
	cheribsd-mips-purecap disk-image-mips-purecap

cochatter : comsg_chatterer.o libcomsg.so
	$(CC) $(CFLAGS)  $(LLDFLAGS) $(LIB_PARAMS) -lcomsg $(INC_PARAMS) \
	-o $(OUTDIR)/cochatter $(BUILDDIR)/comsg_chatterer.o
	$(CHERIBUILD_DIR)/cheribuild.py --skip-update $(FORCE) \
	--cheribsd-mips-purecap/subdir="'usr.bin/cochatter'" \
	--cheribsd/subdir="'usr.bin/cochatter'" \
	cheribsd-mips-purecap disk-image-mips-purecap

comsg_chatterer.o : src/bin/comsg_chatterer.c include/comsg.h \
	$(UKRN_INCDIR)/sys_comsg.h include/coport.h \
	include/comutex.h include/coproc.h include/commap.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/bin/comsg_chatterer.c \
	-o $(BUILDDIR)/comsg_chatterer.o
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/cochatter;)

comesg_kern.o : src/ukernel/comesg_kern.c \
	$(UKRN_INCDIR)/ukern_params.h $(UKRN_INCDIR)/ukern_mman.h \
	$(UKRN_INCDIR)/comesg_kern.h include/coport.h \
	include/comutex.h $(UKRN_INCDIR)/sys_comsg.h \
	$(UKRN_INCDIR)/coport_utils.h $(UKRN_INCDIR)/ukern_commap.h \
	include/coproc.h include/comsg.h $(UKRN_INCDIR)/ukern_utils.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/ukernel/comesg_kern.c \
	-o $(BUILDDIR)/comesg_kern.o
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

libcomsg.so: src/lib/libcomsg.c src/lib/statcounters.c src/lib/coproc.c \
	include/coproc.h include/coport.h  include/comutex.h\
	src/ukernel/include/sys_comsg.h include/comsg.h \
	include/commap.h src/lib/commap.c
	$(CC) $(CFLAGS) $(INC_PARAMS) -shared -o $(OUTDIR)/libcomsg.so src/lib/libcomsg.c src/lib/commap.c src/lib/coproc.c  src/lib/statcounters.c
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

coport_utils.o: $(UKRN_SRCDIR)/coport_utils.c \
	$(UKRN_INCDIR)/coport_utils.h $(UKRN_INCDIR)/sys_comsg.h \
	$(UKRN_INCDIR)/comesg_kern.h include/coport.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/coport_utils.c \
	-o $(BUILDDIR)/coport_utils.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
	cp $< $(CHERIBSD_DIR)/usr.bin/cochatter
endif

ukern_tables.o : $(UKRN_SRCDIR)/ukern_tables.c \
	$(UKRN_INCDIR)/ukern_tables.h $(UKRN_INCDIR)/ukern_mman.h \
	$(UKRN_INCDIR)/ukern_utils.h $(INCDIR)/coport.h \
	$(UKRN_INCDIR)/comesg_kern.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c $(UKRN_SRCDIR)/ukern_tables.c \
	-o $(BUILDDIR)/ukern_tables.o
ifdef CHERIBSD_DIR
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/comesg_ukernel;)
endif

ukern_requests.o : $(UKRN_SRCDIR)/ukern_requests.c \
	$(UKRN_INCDIR)/ukern_requests.h $(UKRN_INCDIR)/ukern_mman.h 
	$(CC) $(CFLAGS) $(INC_PARAMS) -c $(UKRN_SRCDIR)/ukern_requests.c \
	-o $(BUILDDIR)/ukern_requests.o
ifdef CHERIBSD_DIR
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/comesg_ukernel;)
endif

ukern_mman.o : $(UKRN_SRCDIR)/ukern_mman.c \
	$(UKRN_INCDIR)/ukern_mman.h $(UKRN_INCDIR)/ukern_params.h \
	$(UKRN_INCDIR)/sys_comsg.h
	$(CC) $(CFLAGS) $(INC_PARAMS) -c $(UKRN_SRCDIR)/ukern_mman.c \
	-o $(BUILDDIR)/ukern_mman.o
ifdef CHERIBSD_DIR
	$(foreach c, $^, cp $c $(CHERIBSD_DIR)/usr.bin/comesg_ukernel;)
endif

ukern_utils.o: $(UKRN_SRCDIR)/ukern_utils.c \
	$(UKRN_INCDIR)/ukern_utils.h 
	$(CC) $(CFLAGS) $(INC_PARAMS) -c src/lib/ukern_utils.c \
	-o $(BUILDDIR)/ukern_utils.o
ifdef CHERIBSD_DIR
	cp $< $(CHERIBSD_DIR)/usr.bin/comesg_ukernel
endif
