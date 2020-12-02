BUILD_PATH := $(BUILD_DIR)/$(ARCH)/$(TGT)
OUT_PATH := $(OUT_DIR)/$(ARCH)

OBJS := $(SRCS:%=$(BUILD_PATH)/%.o)
DEPS := $(OBJS:.o=.d)
CFLAGS += -MMD -MP
CONFIG := --config cheribsd-$(ARCH)64-purecap.cfg 
LDFLAGS += -L $(OUT_PATH)

ifdef DEP_LIBS
LDFLAGS +=	$(addprefix -l,$(DEP_LIBS))
endif

$(BUILD_DIR)/$(ARCH):
	mkdir $(BUILD_DIR)/$(ARCH)

$(BUILD_PATH): | $(BUILD_DIR)/$(ARCH)
	mkdir $(BUILD_PATH)

$(OUT_PATH):
	mkdir $(OUT_PATH)

$(BUILD_PATH)/%.c.o: %.c | $(BUILD_PATH)
	$(CC) $(CONFIG) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_PATH)/%.S.o: %.S | $(BUILD_PATH)
	$(CC) $(CONFIG) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

$(TGT): $(OBJS) | $(OUT_PATH)
	$(CC) $(CONFIG) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(OUT_PATH)/$(OUT_FILE) 

-include $(DEPS)