# comesg

This is a proof-of-concept userspace microkernel facilitating syscall-free IPC using CHERI co-processes. It was developed by Peter Blandford-Baker for the MPhil in Advanced Computer Science at the Cambridge Computer Lab.

A lot of this code needs tidying, stylising, and neatening. Some parts are untested (comutexes, multiple workers). It can be built for the cocall branch of CheriBSD. If you want it to work, you will also need a version of the coprocess switcher that lets you copy capabilities across protection boundaries from the coaccepting process to the cocalling process. The current version of CheriBSD that works for this is the cocall branch of https://github.com/pentelbart/cheribsd which has a modified switcher capable of copying capabilities.

## Getting Started

Set (and export) the environment variable CHERI_ROOT to the directory containing the directories containing CheriBSD, cheribuild, and the extra-files directory. Alternatively, set CHERI_FSDIR, CHERIBSD_DIR, CHERIBUILD_DIR to the extra-files, Cheribsd, and cheribuild directories respectively. Your CC should be a version of clang able to target 128-bit purecap CheriBSD running on CHERI-MIPS. 

The command: 

`make run` 

will build the code and run QEMU/CheriBSD-purecap. 

WARNING: By default, the `--force` option is enabled which will replace your purecap disk image. If you don't want to do this, set (and export) the FORCE environment variable to an empty value.

An example script setting the environment variables you need can be found in example-setup.sh. Modify this and source it if you want to avoid hassle :)

## Basics

It includes three IPC mechanisms sharing a basic structure (a COPORT). COCHANNELs are short, shared, circular buffers. COCARRIERs are single-copy IPC mechanisms. COPIPEs are also single-copy IPC mechanisms. All three mechanisms are currently 'anycast' (a sent message can be received by one and only one of the listening entities). 

A process sending data via a COCARRIER allocates a buffer, copies their message into that buffer, and places a read-only capability to that buffer in a portion of shared memory provided by the microkernel. The sender relinquishes write capabilities to this buffer so that the message doesn't change once sent.

A process wishing to send data via a COPIPE must wait until a potential recipient makes itself known. The recipient does this by manipulating the status field on the COPORT struct, which resides in shared memory, and by placing a valid capability in the buffer field on the same struct. The sender then copies its data directly into the area described by the provided capability.

There is currently very little security around who can get access to a given COPORT. So long as they know its 'name' within the microkernel managed IPC namespace, they can gain access to it. This access is still constrained by the mechanisms used to communicate using COPORTs. This situation is similar to  Named Pipes, but future work might develop stronger protections for COPORTs. 

COPORTs are all local to a particular instance of the microkernel, and thus, to a single address space. Only one instance of the microkernel can (successfully) receive calls made to the functions it exposes. Inter-address space IPC was outside the scope of this project.


The microkernel provides the following functions:

coopen
coclose
comutex_init
colock
counlock

Of these, only coopen and coclose have been tested. The versions of these functions included in comsg.h and comutex.h are wrappers around invocations of cocall(2) that perform all necessary setup and lookup. Manpages are coming soon(tm).
