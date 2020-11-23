COMSG_DIR ?= $(CURDIR)
BUILD_DIR := $(COMSG_DIR)/build
OUT_DIR := $(COMSG_DIR)/output
SRC_DIR := $(COMSG_DIR)/src
MK_DIR := $(COMSG_DIR)

INC_DIRS += $(COMSG_DIR)/include
INC_FLAGS := $(addprefix -isystem,$(INC_DIRS))

LDFLAGS := -fuse-ld=lld -Wl,-znow -L $(OUT_DIR)
CFLAGS :=  --config cheribsd-mips64-purecap.cfg -g -fPIC -fPIE -v


export MK_DIR BUILD_DIR OUT_DIR CFLAGS LDFLAGS INC_FLAGS

LIBS := $(addprefix lib,$(shell find $(SRC_DIR)/lib -mindepth 1 -type d -exec basename {} \;))
UKERNEL_EXECS := $(shell find $(SRC_DIR)/ukernel -mindepth 1 -type d -exec basename {} \;)
TESTS := $(shell find $(SRC_DIR)/tests -mindepth 1 -type d -exec basename {} \;)


libcoproc:
	$(MAKE) -C $(SRC_DIR)/lib/$(subst lib,,$@) $@
	cp $(OUT_DIR)/$@.so.* $(OUT_DIR)/$@.so
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib/$@.so

libcocall: libcoproc
	$(MAKE) -C $(SRC_DIR)/lib/$(subst lib,,$@) $@
	cp $(OUT_DIR)/$@.so.* $(OUT_DIR)/$@.so
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib/$@.so

libcomsg: libcoproc libcocall
	$(MAKE) -C $(SRC_DIR)/lib/$(subst lib,,$@) $@
	cp $(OUT_DIR)/$@.so.* $(OUT_DIR)/$@.so
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib/$@.so

libccmalloc: 
	$(MAKE) -C $(SRC_DIR)/lib/$(subst lib,,$@) $@
	cp $(OUT_DIR)/$@.so.* $(OUT_DIR)/$@.so
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib
	cp $(OUT_DIR)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib/$@.so

.PHONY: libs
libs: $(LIBS) 

$(UKERNEL_EXECS): libs
	$(MAKE) -C $(SRC_DIR)/ukernel/$@ $@
	cp $(OUT_DIR)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: ukernel
ukernel : libs $(UKERNEL_EXECS)

$(TESTS): libs ukernel
	$(MAKE) -C $(SRC_DIR)/tests/$@ $@
	cp $(OUT_DIR)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: tests
tests : libs ukernel $(TESTS)

.PHONY: clean
clean : 
	rm -r build/*
	rm -r output/*