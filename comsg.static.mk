BUILD_PATH := $(BUILD_DIR)/$(ARCH)/$(TGT)
OUT_PATH := $(OUT_DIR)/$(ARCH)

OBJS := $(SRCS:%=$(BUILD_PATH)/%.o)
DEPS := $(OBJS:.o=.d)
CFLAGS += -MD -MP

ifeq ($(ARCH),arm)
SDK := $(MORELLO_SDK_DIR)
TGT_CFG := morello
else
SDK := $(CHERI_SDK_DIR)
TGT_CFG := $(ARCH)64
endif

CONFIG := --config $(SDK)/bin/cheribsd-$(TGT_CFG)-purecap.cfg
PATH := $(SDK)/bin:$(PATH)

ifdef DEP_LIBS
LDFLAGS +=	$(addprefix -l,$(DEP_LIBS))
endif

export PATH

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