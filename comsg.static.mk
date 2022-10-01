BUILD_PATH := $(BUILD_DIR)/$(ARCH)/$(TGT)
OUT_PATH := $(OUT_DIR)/$(ARCH)

OBJS := $(SRCS:%=$(BUILD_PATH)/%.o)
DEPS := $(OBJS:.o=.d)
CFLAGS += -MD -MP
CONFIG := -target riscv64-unknown-freebsd13 --sysroot=$(CHERI_ROOT)/output/rootfs-riscv64-purecap -B$(cheri_sdk_dir)/bin -march=rv64imafdcxcheri -mabi=l64pc128d -mno-relax

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
$(BUILD_PATH)/%.d: %.c
	$(CC) $(CONFIG) $(INC_FLAGS) -I$(realpath $(<D)) $(CFLAGS) -c $< -MMD -MP -MF $@

.SECONDEXPANSION:
$(BUILD_PATH)/%.c.o: %.c | $(BUILD_PATH)/. $$(@D)/.
	$(CC) $(CONFIG) $(INC_FLAGS) -I$(realpath $(<D)) $(CFLAGS) -c $< -o $@

.SECONDEXPANSION:
$(BUILD_PATH)/%.S.o: %.S | $(BUILD_PATH)/. $$(@D)/.
	$(CC) $(CONFIG) $(INC_FLAGS) -I$(realpath $(<D)) $(CFLAGS) -c $< -o $@

$(TGT): $(OBJS) | $(OUT_PATH)
	$(AR) rcs $(OUT_PATH)/$(OUT_FILE) $(OBJS)

-include $(DEPS)