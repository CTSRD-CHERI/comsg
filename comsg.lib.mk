TGT = lib$(LIB)
OUT_FILE = $(TGT).so.$(SHLIB_MAJOR)

LDFLAGS += -shared


include $(MK_DIR)/comsg.src.mk


