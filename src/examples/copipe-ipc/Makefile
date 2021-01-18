export MK_DIR BUILD_DIR OUT_DIR CFLAGS LDFLAGS INC_FLAGS

sender:
	$(MAKE) -C $(CURDIR)/copipe_recv copipe-ipc ARCH=$($@_ARCH)

receiver:
	$(MAKE) -C $(CURDIR)/copipe_send copipe-ipc2 ARCH=$($@_ARCH)

copipe-ipc: sender receiver

