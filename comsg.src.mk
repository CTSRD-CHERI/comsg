BUILD_PATH := $(BUILD_DIR)/$(TGT)

OBJS := $(SRCS:%=$(BUILD_PATH)/%.o)
DEPS := $(OBJS:.o=.d)
CFLAGS += -MMD -MP

ifdef DEP_LIBS
LDFLAGS +=	$(addprefix -l,$(DEP_LIBS))
endif

$(BUILD_PATH):
	mkdir $(BUILD_PATH)

$(BUILD_PATH)/%.c.o: %.c | $(BUILD_PATH)
	$(CC) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_PATH)/%.S.o: %.S | $(BUILD_PATH)
	$(CC) $(INC_FLAGS) $(CFLAGS) -c $< -o $@

$(TGT): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(OUT_DIR)/$(OUT_FILE) 

-include $(DEPS)