COMSG_DIR ?= 	$(CURDIR)
SHELL := /bin/bash

BUILD_DIR := 	$(COMSG_DIR)/build
OUT_DIR := 		$(COMSG_DIR)/output
SRC_DIR := 		$(COMSG_DIR)/src
MK_DIR := 		$(COMSG_DIR)

INC_DIRS +=		$(COMSG_DIR)/include
INC_FLAGS :=	$(addprefix -isystem,$(INC_DIRS))
LDFLAGS :=		-fuse-ld=lld -Wl,-znow 
CFLAGS :=		-g

SUPPORTED_TARGETS := riscv arm
DEFAULT_ARCH := riscv
ifndef ARCH
	ifndef TARGETS
		TARGETS := $(SUPPORTED_TARGETS)
		ARCH := $(DEFAULT_ARCH)
	else
		temp_targets := $(foreach arch,$(TARGETS),$(findstring $(arch),$(SUPPORTED_TARGETS)))
		ifneq ($(TARGETS), $(temp_targets))
			$(error Invalid set of targets [$(TARGETS)] was supplied. Supported targets are: $(SUPPORTED_TARGETS))
		endif
		ARCH := $(word 1,$(TARGETS))
	endif
else
	ifndef TARGETS
		ifneq ($(findstring $(ARCH),$(SUPPORTED_TARGETS)), $(ARCH))
			$(error Supplied architecture [$(ARCH)] not in list of supported architectures [$(TARGETS)])
		endif
		TARGETS := $(ARCH)
	else
		temp_targets := $(foreach arch,$(TARGETS),$(findstring $(arch),$(SUPPORTED_TARGETS)))
		ifneq ($(TARGETS), $(temp_targets))
			$(error Invalid set of targets [$(TARGETS)] was supplied. Supported targets are: $(SUPPORTED_TARGETS))
		endif
		ifneq ($(findstring $(ARCH),$(TARGETS)), $(ARCH))
			$(error Supplied architecture [$(ARCH)] not in provided list of architectures [$(TARGETS)])
		endif
	endif
endif

export MK_DIR BUILD_DIR OUT_DIR CFLAGS LDFLAGS INC_FLAGS SHELL

LIBS := $(addprefix lib,$(shell find $(SRC_DIR)/lib -mindepth 1 -maxdepth 1 -type d -exec basename {} \;))
ARCH_LIBS := $(foreach arch,$(TARGETS),$(addsuffix -$(arch),$(LIBS)))

UKERNEL_EXECS := $(shell find $(SRC_DIR)/ukernel -mindepth 1 -maxdepth 1 -type d -exec basename {} \;)
ARCH_EXECS := $(foreach arch,$(TARGETS),$(addsuffix -$(arch),$(UKERNEL_EXECS)))

TESTS := $(shell find $(SRC_DIR)/tests -mindepth 1 -type d -exec basename {} \;)
ARCH_TESTS := $(foreach arch,$(TARGETS),$(addsuffix -$(arch),$(TESTS)))

EXAMPLES := $(shell find $(SRC_DIR)/examples -mindepth 1 -maxdepth 1 -type d -exec basename {} \;)
ARCH_EXAMPLES := $(foreach arch,$(TARGETS),$(addsuffix -$(arch),$(EXAMPLES)))

ARCH_TGTS := $(ARCH_LIBS) $(ARCH_EXECS) $(ARCH_TESTS) $(ARCH_EXAMPLES)
INSTALL_ROOTS := $(addprefix $(CHERI_ROOT)/, extra-files extra-files-minimal)
INSTALL_PARENTS := $(addsuffix /usr, $(INSTALL_ROOTS))
INSTALL_DIRS := $(addsuffix /lib, $(INSTALL_PARENTS))
INSTALL_DIRS += $(addsuffix /bin, $(INSTALL_PARENTS))

#gosh this is awful
.PHONY: $(ARCH_TGTS)
$(ARCH_TGTS):
	$(eval $@_WORDS	:= $(subst -, ,$@))
	$(eval $@_ARCH 	:= $(lastword $($@_WORDS)))
	$(eval $@_TGT	:= $(subst -$($@_ARCH),,$@))
	$(eval $@_DIR	:= $(patsubst $($@_TGT),examples,$(patsubst %test,tests,$(patsubst %d,ukernel,$(patsubst lib%,lib,$($@_TGT))))))
	$(eval $@_NAME	:= $(patsubst lib%,%,$($@_TGT)))
	$(MAKE) -s -C $(SRC_DIR)/$($@_DIR)/$($@_NAME) $($@_TGT) ARCH=$($@_ARCH)
	@if compgen -G "$(OUT_DIR)/$($@_ARCH)/$($@_TGT).so.*" > /dev/null; then \
	cp $(OUT_DIR)/$($@_ARCH)/$($@_TGT).so.* $(OUT_DIR)/$($@_ARCH)/$($@_TGT).so; \
	fi

$(INSTALL_DIRS) : | $(INSTALL_PARENTS)
	mkdir -p $@

$(INSTALL_PARENTS) : | $(INSTALL_ROOTS)
	mkdir -p $@

$(INSTALL_ROOTS):
	mkdir -p $@

.SECONDEXPANSION:
$(LIBS): $$(addprefix $$@-,$$(TARGETS)) | $(INSTALL_DIRS)
	@cp $(OUT_DIR)/$(ARCH)/$@* $(CHERI_ROOT)/extra-files/usr/lib
	@cp $(OUT_DIR)/$(ARCH)/$@* $(CHERI_ROOT)/extra-files-minimal/usr/lib
	@echo Made $@.

.PHONY: libs
libs: $(LIBS)

.SECONDEXPANSION:
$(UKERNEL_EXECS): libs $$(addprefix $$@-,$$(TARGETS)) | $(INSTALL_DIRS)
	@cp $(OUT_DIR)/$(ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin
	@cp $(OUT_DIR)/$(ARCH)/$@ $(CHERI_ROOT)/extra-files-minimal/usr/bin
	@echo Made $@.

.PHONY: ukernel
ukernel : libs $(UKERNEL_EXECS)

.SECONDEXPANSION:
$(TESTS): libs ukernel $$(addprefix $$@-,$$(TARGETS)) | $(INSTALL_DIRS)
	@cp $(OUT_DIR)/$(ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin
	@cp $(OUT_DIR)/$(ARCH)/$@ $(CHERI_ROOT)/extra-files-minimal/usr/bin
	@echo Made $@.

.PHONY: tests
tests : libs ukernel $(TESTS)

.SECONDEXPANSION:
$(EXAMPLES): libs ukernel $$(addprefix $$@-,$$(ARCHES)) | $(INSTALL_DIRS)
	@cp $(OUT_DIR)/$(ARCH)/$@ $(CHERI_ROOT)/extra-files/usr/bin
	@cp $(OUT_DIR)/$(ARCH)/$@ $(CHERI_ROOT)/extra-files-minimal/usr/bin
	@echo Made $@.

.PHONY: examples
examples: libs ukernel $(EXAMPLES)

.PHONY: clean
clean : 
	rm -r build/*
	rm -r output/*

.PHONY: all
all : examples tests
	@echo Made $@.
