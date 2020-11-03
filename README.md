# comesg

This is a proof-of-concept userspace microkernel facilitating syscall-free IPC using CHERI co-processes.

Co-processes are an experimental feature of CheriBSD whereby multiple userspace processes share the same virtual address space. Processes running in this way remain fully separate within the operating system and from each other, thanks to the spatial memory safety constraints enforced by the CHERI architecural features. CheriBSD's CheriABI is responsible for ensuring that user code never gains access to the full address space, and in particular never receives capabilities for any memory mapping that hasn't been explicitly granted to that process.  For each address space, kernel provides the switcher as part of the Shared Page. The switcher allows user code to perform a syscall-free domain transition (a "cocall") between the accessible memory regions of the two processes, with the caller "borrowing" the callee thread's context. Performing a cocall requires the caller to hold a capability authorising access to the callee. If required, the full OS kernel can lazily catch up ("unborrow") if a system call or other trap takes place. Via the switcher, different user processes can share capabilities to portions of their own address space with other processes, permitting shared memory not only using the same physical memory, but also the same page-table and TLB entries.

The co-process microkernel is itself compartmentalized, running over the switcher, and uses the cocall mechanism to provide functionality to user programs like the kernel does.  Instead of system calls, however, user programs perform cocalls to use microkernel functions.  Microkernel-based services include memory management, service management, event management, high-performance IPC, and IPC namespaces.  User processes continue to have access to the services of the general-purpose OS kernel -- at the normal cost of system calls.  

## Getting Started

This requires CheriBSD from the cocall-copycap branch of https://github.com/CTSRD-CHERI/cheribsd which has a modified switcher capable of copying capabilities.

Set (and export) the environment variable CHERI_ROOT to the directory containing the directories containing CheriBSD, cheribuild, and the extra-files directory. Alternatively, set CHERI_FSDIR, CHERIBSD_DIR, CHERIBUILD_DIR to the extra-files, Cheribsd, and cheribuild directories respectively. Your CC should be a version of clang able to target 128-bit purecap CheriBSD running on CHERI-MIPS. (Support for CHERI-RISCV is currently a work-in-progress)

The command: 

`make tests`

will build the the binaries and copy them to the extra-files directory. Cheribuild can then be used to build a new disk image that contains the binaries. 

WARNING: By default, the `--force` option is enabled which will replace your purecap disk image. If you don't want to do this, set (and export) the FORCE environment variable to an empty value.

An example script setting the environment variables you need can be found in example-setup.sh. Modify this and source it if you want to avoid hassle :)

## Concepts

NOTE: This section is mostly incomplete.

### Coports - Fast Userspace IPC

The microkernel compartment *ipcd* provides fast IPC to user programs. The three IPC mechanisms provided share a basic structure (a coport). COCHANNELs are short, shared, circular buffers. COCARRIERs are single-copy IPC mechanisms. COPIPEs are direct-copy IPC mechanisms. All three mechanisms are currently 'anycast' (a sent message can be received by one and only one of the listening entities). 

A process sending data via a COCARRIER must call into the microkernel, passing a capability to its message, a handle to a coport, and the length of message they wish to send. The microkernel copies the message into memory that it owns, and places a read-only capability to that message into a queue. To receive a message, a process calls into the microkernel and removes this capability from the queue. COCARRIERs support event monitoring via a poll-like microkernel call.

A process wishing to send data via a COPIPE must wait until a potential recipient makes itself known. The recipient signals its availability via the status field on the COPORT struct after placing a valid capability in the buffer field on the same struct. The sender then directly writes its message via the provided capability.

COPORTs are all local to a particular instance of the microkernel, and thus, to a single address space. Only one instance of the microkernel can run in each address space. 

### Namespace Management

NOTE: WIP

The structure of the microkernel namespace tree is similar to the UNIX filesystem. Namespaces are analogous to directories, while namespace objects are like files. Namespace objects have a type and a textual name, and contain a handle to a resource, such as a coport or cocall-based service. Namespaces may contain namespace objects and other namespaces. They have a textual name and type.

### Coservices

Coservices allow multiple threads to provide the same functionality via cocalls. The capability to perform a cocall only pertains to a specific thread, and so coservices were implemented to avoid bottlenecks for frequently used cocall targets. 

## Functions

Three shared libraries, libcoproc, licocall, and libcomsg, are provided for interacting with coprocesses, the microkernel and IPC mechanisms. The library provides the following functions:

### Setup Functions

+ coproc_init - retrieves capabilities to call coselect, coinsert, and codiscover

### IPC Functions

+ open_coport - open a new coport of a specified type
+ open_named_coport - open a new named (name+namespace) coport of a specified type
+ coclose - close a coport

+ cosend - send data over a coport
+ corecv - receive data via a coport
+ copoll - inspect the event state of a coport

### Namespace Functions

+ cocreate - create a new (child) namespace
+ codrop - delete a namespace (currently not fully implemented)

+ coinsert - create a named object with specified type in a namepsace
+ coselect - search a namespace for an object with matching name and type 
+ codelete - delete a namespace object
+ coupdate - convert a namespace reservation to an "active" type

### Service Management Functions

+ codiscover - retrieve a capability to cocall the service specified
+ coprovide - set up a service provision

