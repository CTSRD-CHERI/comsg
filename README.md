# comesg

## What we currently have:

1. Basic microkernel/ipc functionality 
	* we currently use a generic coport_t struct with an enum 'type' field for all flavours
	* semantics are a bit fiddly with cocall, but I've made them friendlier 
	* supported operations:
		+ create new ipc channel
		+ lookup existing ipc channel
	* infrastructure:
		+ table tracking coports in microkernel 
		+ support for multi-threading in microkernel to expose multiple functions
			- named function is cocalled into
			- this function returns the capability required to cocall the worker thread handling calls to the "real" function
			- ideally we only do this once per named function per caller
			- worker allocations are not smart (but are predictable; possible side channel?)
2. Three ipc types
	* all currently follow anycast behaviour (one to one of many)
		+ or at least, they should, if we can make the necessary things atomic/safe
	* COCHANNEL 
		+ shared circular buffer.
	* COCARRIER 
		+ single-copy message passing 
		+ message is copied into a read only buffer to which sender relinquishes store capability over
		+ receive operation grants recipient a load capability.
	* COPIPE 
		+ intended to be synchronous, behave mostly like a pipe.
		+ might replace COCHANNEL if these start converging for name recognition.
3. comutexes
	* cross-process mutexes 
	* very much unfinished, do not work, and are not real... yet.
		+ there is some code that definitely doesn't work because I don't know how to atomic compare-and-swap capabilities
	* I am not 100% decided on how/if we should do these yet
		+ we need *something* like this, even if it's cmpset based or similar
		+ might just be calls into the microkernel
		+ might use capability sealing/unsealing as locking/unlocking
			- could very easily manage shared objects this way
			- might want to split these into two things

## Current outstanding questions:

1. How to make certain operations atomic so that we don't mess up during key stages
	* atomic compare and swap for capabilities? does this exist?
2. How to ensure that we clean up after threads when they exit
	* atexit only works in normal exits
	* microkernel could maintain a list of threads that have called into it
		* periodically check whether these have exited
		* atexit could delete a thread from this list after cleanup
3. *Should* COCHANNEL messages be read-only?
	* when do we deallocate the buffers? memory leakage is a concern.
	* programs might want to do things with the data besides read, and we should try and support this.

## What we would like from cocall/should find a way to implement:

1. Not losing the switcher code/data capabilities down the back of the sofa
	* cosetup(2) gives us these even if we do lose them, but this is a syscall
	* not super vital, but would make the semantics friendlier IMO

## Changes we have made to cocall/other aspects

1. re-register dead names
2. "reference" counting for coports
	* keep track of how many people could be using a coport
	* if that number hits 0, we can consider removing it

## Definitely TODO:

1. Apply principle of least privileges to coport_t struct members.
2. Smooth things out; I believe there are currently a lot of rough edges
	* particularly around who has capabilities to what
	* Some operations which currently happen 'locally' might be better implemented as calls to the microkernel

## Possibly TODO:

1. Develop an interface for programs to use the multi-consumer cocall we use to handle calls to the microkernel
	* effectively, service registration
	* microkernel receives the call, returns a string