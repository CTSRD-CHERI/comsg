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

EXAMPLES := $(shell find $(SRC_DIR)/examples -mindepth 1 -maxdepth 1 -type d -exec basename {} \;)
ARCH_EXAMPLES := $(foreach arch,$(ARCHES),$(addsuffix -$(arch),$(EXAMPLES)))

ARCH_TGTS := $(ARCH_LIBS) $(ARCH_EXECS) $(ARCH_TESTS) $(ARCH_EXAMPLES)

#gosh this is awful
.PHONY: $(ARCH_TGTS)
$(ARCH_TGTS):
	$(eval $@_WORDS	:= $(subst -, ,$@))
	$(eval $@_ARCH 	:= $(lastword $($@_WORDS)))
	$(eval $@_TGT	:= $(subst -$($@_ARCH),,$@))
	$(eval $@_DIR	:= $(patsubst $($@_TGT),examples,$(patsubst %test,tests,$(patsubst %d,ukernel,$(patsubst lib%,lib,$($@_TGT))))))
	$(eval $@_NAME	:= $(patsubst lib%,%,$($@_TGT)))
	$(MAKE) -C $(SRC_DIR)/$($@_DIR)/$($@_NAME) $($@_TGT) ARCH=$($@_ARCH)
	@if [ "$($@_DIR)" == "lib" ]; then \
	cp $(OUT_DIR)/$($@_ARCH)/$($@_TGT).so.* $(OUT_DIR)/$($@_ARCH)/$($@_TGT).so; \
	fi

.SECONDEXPANSION:
$(LIBS): $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@.so.* $(CHERI_ROOT)/extra-files/usr/lib
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@.so $(CHERI_ROOT)/extra-files/usr/lib/$@.so

.PHONY: libs
libs: $(LIBS)

.SECONDEXPANSION:
$(UKERNEL_EXECS): libs $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: ukernel
ukernel : libs $(UKERNEL_EXECS)

.SECONDEXPANSION:
$(TESTS): libs ukernel $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: tests
tests : libs ukernel $(TESTS)

.SECONDEXPANSION:
$(EXAMPLES): libs ukernel $$(addprefix $$@-,$$(ARCHES))
	cp $(OUT_DIR)/$(DEFAULT_ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin

.PHONY: examples
examples: libs ukernel $(EXAMPLES)

.PHONY: clean
clean : 
	rm -r build/*
	rm -r output/*