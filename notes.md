
## What we currently have:

1. Basic microkernel/ipc functionality 
	* We currently use a generic coport_t struct with an enum 'type' field for all flavours
	* Semantics are a bit fiddly with cocall, but I've made them friendlier 
	* Supported operations:
		+ Create new ipc channel
		+ Lookup existing ipc channel
		+ Create comutex
		+ Lock/unlock comutex
		+ Lookup existing comutex
	* Infrastructure:
		+ Tables tracking coports and comutexes in microkernel 
		+ Support for multi-threading in microkernel to expose multiple functions
			- Named function is cocalled into (e.g. coopen or colock)
			- This function returns the capability required to cocall the worker thread. This thread actually does the work.
			- Ideally, we only do this once per named function per caller
			- Worker allocations are not smart (but are predictable; possible side channel/route for targeted Denial-of-service)
2. Three ipc types
	* All currently follow anycast behaviour (one to one of many)
		+ Or at least, they should; I haven't verified atomicity of the operations to guarantee this
		+ This is on a per-message basis; there may be many senders over multiple messages.
	* COCHANNEL 
		+ Shared circular buffer, by default 4096B to align with page size.
	* COCARRIER 
		+ Single-copy message passing
		+ Message is copied into a read only buffer to which sender relinquishes store capability over
		+ Receive operation grants recipient a load capability.
	* COPIPE 
		+ Intended to be synchronous, behave mostly like a pipe.
		+ Might replace COCHANNEL if these start converging for name recognition.
3. comutexes
	* Cross-process mutexes 
	* These need a bit of reworking at present, there were two lines of thinking that converged with remnants of the abandoned parts in the actual implementation
	* Locking, setup, and unlocking go through the microkernel
	* When they're locked, the write capability to the lock variable is sealed
	* The sealing capability is kept by both the microkernel and the sealing thread
		+ The microkernel keeps it in case the thread exits without releasing the comutex
	* Could very easily manage shared objects similarly
		+ This could form the basis of another IPC primitive.
	* The current line of thinking is:
		+ Setup of mutual exclusion locks will be managed by the microkernel, similar to POSIX semaphores.
		+ Comutexes are named in the microkernel.
		+ Sealing is used to avoid syscall-based checks for thread ID or similar.

## Current outstanding questions:


1. How can we ensure that we clean up after threads when they exit?
	* atexit only works for 'normal' exits
	* Microkernel could maintain a list of threads that have called into it
		+ Periodically check whether these have exited?
			- Perhaps by inspecting SCBs
		+ atexit could delete a thread from this list after cleanup
2. *Should* COCARRIER messages be read-only?
	* When do we deallocate the buffers? memory leakage is a concern.
	* Programs might want to do things with the data besides read, and we should try and support this.
		+ This might be fine, as they can simply copy messages out if they want to do this. 
3. Why doesn't it work? :/
	* VM issues
		+ COCARRIER messages are unmapped on sender exit()
			- Abdicate ownership?
			- Transfer ownership?
			- Accept this - maybe this should happen?
			- ATEXIT checks/usurpation of ownership by ukernel of pending msgs


## What we would like from cocall et al/should find a way to implement:

1. Not losing the switcher code/data capabilities down the back of the sofa
	* cosetup(2) gives us these even if we do lose them, but this is a syscall
	* Not super vital, but would make the semantics friendlier IMO
2. The error condition for a dead callee in cheri_cocall.S does not work

## Changes we have made to cocall et al

1. re-register dead names (https://github.com/CTSRD-CHERI/cheribsd/pull/392)
	* Previously, coregister(2) didn't allow you to use names registered previously by now dead threads
		- This made testing a bit of a pain, so I reworked the bit where this happens to check if the thread has died.
2. modify switcher to copy capabilities with tags across cocall/coaccept

## Probably TODO:

1. Apply principle of least privileges to coport_t struct members.
	* When implementing comutexes, I separated out some struct members based on their required permissions. I think this is a good idea and would like to apply this to coports.
2. Smooth things out; I believe there are currently a lot of rough edges
	* Particularly around who has capabilities to what
	* Some operations which currently happen 'locally' might be better implemented as calls to the microkernel 
		+ This was done for the comutex implementation
3. "Reference" counting for coports
	* Keep track of how many 'users' could be using a coport
	* if that number hits 0, we can consider removing it

## Possibly TODO:

1. Develop an interface for programs to use the multi-consumer cocall we use to handle calls to the microkernel
	* effectively, service registration
	* microkernel receives the call, returns a string or capability like colookup(2)