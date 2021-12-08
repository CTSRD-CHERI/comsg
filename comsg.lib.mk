TGT = lib$(LIB)
OUT_FILE = $(TGT).so.$(SHLIB_MAJOR)

CFLAGS += -fPIC
LDFLAGS += -shared


include $(MK_DIR)/comsg.src.mk


