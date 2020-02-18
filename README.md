# comessages

## Current outstanding questions:

1. How to make operations atomic so that we don't mess up during key stages
	* atomic compare and swap for capabilities? does this exist?
2. How to ensure that we clean up after threads when they exit
	* atexit only works in normal situations
	* microkernel could maintain a list of threads that have called into it
		* periodically check whether these have exited


## What we currently have:

1. Basic microkernel/ipc functionality 
	* we currently use a generic coport_t struct with a type field for all varieties
	* semantics are a bit fiddly with cocall, but I've made them friendlier 
	* supported operations:
		+ create new ipc channel
		+ lookup existing ipc channel
	* infrastructure:
		+ table tracking coports in microkernel 
		+ support for multi-threading in microkernel to expose multiple functions
2. Three ipc types
	* COCHANNEL - page-sized shared buffer, some elements still a WIP.
	* COCARRIER - single-copy message passing - message is copied into a read only buffer to which the recipient gets a load capability.
	* COPIPE - intended to be synchronous, behave like a pipe

## What we would like from cocall/should find a way to implement:



## Definitely TODO:

1. Apply principle of least privileges to coport_t struct members.