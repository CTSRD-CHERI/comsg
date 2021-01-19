export MK_DIR BUILD_DIR OUT_DIR CFLAGS LDFLAGS INC_FLAGS ARCH

sender:
	$(MAKE) -C $(CURDIR)/copipe_recv copipe-ipc 

receiver:
	$(MAKE) -C $(CURDIR)/copipe_send copipe-ipc2

copipe_ipc: sender receiver

