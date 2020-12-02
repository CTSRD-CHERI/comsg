COMSG_DIR ?= 	$(CURDIR)
ARCHES := riscv mips 
DEFAULT_ARCH ?= riscv
BUILD_DIR := 	$(COMSG_DIR)/build
OUT_DIR := 		$(COMSG_DIR)/output
SRC_DIR := 		$(COMSG_DIR)/src
MK_DIR := 		$(COMSG_DIR)

INC_DIRS +=		$(COMSG_DIR)/include
INC_FLAGS :=	$(addprefix -isystem,$(INC_DIRS))

LDFLAGS :=		-fuse-ld=lld -Wl,-znow 
CFLAGS :=		-g -fPIC -fPIE -v

export MK_DIR BUILD_DIR OUT_DIR CFLAGS LDFLAGS INC_FLAGS

LIBS := $(addprefix lib,$(shell find $(SRC_DIR)/lib -mindepth 1 -maxdepth 1 -type d -exec basename {} \;))
ARCH_LIBS := $(foreach arch,$(ARCHES),$(addsuffix -$(arch),$(LIBS)))

UKERNEL_EXECS := $(shell find $(SRC_DIR)/ukernel -mindepth 1 -type d -exec basename {} \;)
ARCH_EXECS := $(foreach arch,$(ARCHES),$(addsuffix -$(arch),$(UKERNEL_EXECS)))

TESTS := $(shell find $(SRC_DIR)/tests -mindepth 1 -type d -exec basename {} \;)
ARCH_TESTS := $(foreach arch,$(ARCHES),$(addsuffix -$(arch),$(TESTS)))

ARCH_TGTS := $(ARCH_LIBS) $(ARCH_EXECS) $(ARCH_TESTS)

$(ARCH_TGTS):
	@echo $(ARCH_TGTS)
	$(eval $@_WORDS	:= $(subst -, ,$@))
	$(eval $@_ARCH 	:= $(lastword $($@_WORDS)))
	$(eval $@_TGT	:= $(firstword $($@_WORDS)))
	$(eval $@_DIR	:= $(patsubst %test,tests,$(patsubst lib%,lib, $(patsubst %d,ukernel,$($@_TGT)))))
	$(eval $@_NAME	:= $(patsubst lib%,%,$($@_TGT)))
	$(MAKE) -C $(SRC_DIR)/$($@_DIR)/$($@_NAME) $($@_TGT) ARCH=$($@_ARCH)
ifeq ($($@_DIR), lib)
	cp $(OUT_DIR)/$($@_ARCH)/$($@_TGT).so.* $(OUT_DIR)/$($@_ARCH)/$($@_TGT).so
endif


.SECONDEXPANSION:
$(LIBS): $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib/$@.so

.PHONY: libs
libs: $(LIBS)

.SECONDEXPANSION:
$(UKERNEL_EXECS): libs $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: ukernel
ukernel : libs $(UKERNEL_EXECS)

$(TESTS): libs ukernel $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: tests
tests : libs ukernel $(TESTS)

.PHONY: clean
clean : 
	rm -r build/*
	rm -r output/*