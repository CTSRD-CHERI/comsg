BUILD_PATH := $(BUILD_DIR)/$(ARCH)/$(TGT)
OUT_PATH := $(OUT_DIR)/$(ARCH)

OBJS := $(SRCS:%=$(BUILD_PATH)/%.o)
DEPS := $(OBJS:.o=.d)
CFLAGS += -MMD -MP
CONFIG := -target riscv64-unknown-freebsd13 --sysroot=/Users/peter/Projects/CHERI/output/rootfs-riscv64-purecap -B/Users/peter/Projects/CHERI/output/sdk/bin -march=rv64imafdcxcheri -mabi=l64pc128d -mno-relax
LDFLAGS += -L $(OUT_PATH)

ifdef DEP_LIBS
LDFLAGS +=	$(addprefix -l,$(DEP_LIBS))
endif

.PRECIOUS: $(BUILD_PATH)/. $(BUILD_PATH)%/.

$(BUILD_DIR)/$(ARCH):
	mkdir -p $@

$(BUILD_PATH)/.: | $(BUILD_DIR)/$(ARCH)
	mkdir -p $@

$(BUILD_PATH)%/.: | $(BUILD_PATH)
	mkdir -p $@

$(OUT_PATH):
	mkdir -p $@

.SECONDEXPANSION:
$(BUILD_PATH)/%.c.o: %.c | $(BUILD_PATH)/. $$(@D)/.
	$(CC) $(CONFIG) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

.SECONDEXPANSION:
$(BUILD_PATH)/%.S.o: %.S | $(BUILD_PATH)/. $$(@D)/.
	$(CC) $(CONFIG) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

$(TGT): $(OBJS) | $(OUT_PATH)
	$(CC) $(CONFIG) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(OUT_PATH)/$(OUT_FILE) 

-include $(DEPS)