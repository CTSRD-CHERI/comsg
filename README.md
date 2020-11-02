# comesg

This is a proof-of-concept userspace microkernel facilitating syscall-free IPC using CHERI co-processes. Co-processes are an experimental feature of CheriBSD whereby multiple userspace processes share the same virtual address space. Processes running in this way remain fully separate within the operating system and from each other, thanks to the spatial memory safety constraints enforced by the CHERI architecural features. For each address space, kernel provides the switcher as part of the Shared Page. The switcher allows user code to perform a syscall-free domain transition (a "cocall"). The microkernel uses this mechanism to provide functionality to user programs like the kernel does; instead of system calls, however, user programs perform cocalls to use microkernel functions.

## Getting Started

This requires CheriBSD from the cocall-copycap branch of https://github.com/CTSRD-CHERI/cheribsd which has a modified switcher capable of copying capabilities.

Set (and export) the environment variable CHERI_ROOT to the directory containing the directories containing CheriBSD, cheribuild, and the extra-files directory. Alternatively, set CHERI_FSDIR, CHERIBSD_DIR, CHERIBUILD_DIR to the extra-files, Cheribsd, and cheribuild directories respectively. Your CC should be a version of clang able to target 128-bit purecap CheriBSD running on CHERI-MIPS. (Support for CHERI-RISCV is currently a work-in-progress)

The command: 

`make tests`

will build the the binaries and copy them to the extra-files directory. Cheribuild can then be used to build a new disk image that contains the binaries. 

WARNING: By default, the `--force` option is enabled which will replace your purecap disk image. If you don't want to do this, set (and export) the FORCE environment variable to an empty value.

An example script setting the environment variables you need can be found in example-setup.sh. Modify this and source it if you want to avoid hassle :)

## Basics

NOTE: This section is mostly incomplete.

### Coports

Included are three IPC mechanisms sharing a basic structure (a COPORT). COCHANNELs are short, shared, circular buffers. COCARRIERs are single-copy IPC mechanisms. COPIPEs are also single-copy IPC mechanisms. All three mechanisms are currently 'anycast' (a sent message can be received by one and only one of the listening entities). 

A process sending data via a COCARRIER allocates a buffer, copies their message into that buffer, and places a read-only capability to that buffer in a portion of shared memory provided by the microkernel. The sender relinquishes write capabilities to this buffer so that the message doesn't change once sent.

A process wishing to send data via a COPIPE must wait until a potential recipient makes itself known. The recipient does this by manipulating the status field on the COPORT struct, which resides in shared memory, and by placing a valid capability in the buffer field on the same struct. The sender then copies its data directly into the area described by the provided capability.

COPORTs are all local to a particular instance of the microkernel, and thus, to a single address space. Only one instance of the microkernel can run in each address space. Inter-address space IPC was outside the scope of this project.

### Namespaces and namespace objects

Implemented (mostly). Description TODO.

### Coservices

Implemented (barebones). Description TODO.

## Functions

Three shared libraries, libcoproc, licocall, and libcomsg, are provided for interacting with coprocesses, the microkernel and IPC mechanisms. The library provides the following functions:

### Setup Functions

+coproc_init - retrieves capabilities to call coselect, coinsert, and codiscover

### IPC Functions

+open_coport - open a new coport of a specified type
+open_named_coport - open a new named (name+namespace) coport of a specified type
+coclose - close a coport

+cosend - send data over a coport
+corecv - receive data via a coport
+copoll - inspect the event state of a coport

### Namespace Functions

+cocreate - create a new (child) namespace
+codrop - delete a namespace (currently not fully implemented)

+coinsert - create a named object with specified type in a namepsace
+coselect - search a namespace for an object with matching name and type 
+codelete - delete a namespace object
+coupdate - convert a namespace reservation to an "active" type

### Service Management Functions

+codiscover - retrieve a capability to cocall the service specified
+coprovide - set up a service provision

