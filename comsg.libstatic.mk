TGT = lib$(LIB)
OUT_FILE = $(TGT).a

CFLAGS += -fPIC
LDFLAGS += -static

include $(MK_DIR)/comsg.static.mk


