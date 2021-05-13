#Sample Makefile to demonstrate how to compile the Emu examples

all: hello-world.mwx hello-world-spawn.mwx hello-world-spawn-at.mwx

.PHONY: sim
sim: hello-world.tqd hello-world-spawn.tqd hello-world-spawn-at.tqd

%.mwx: %.c
	emu-cc -o $@ $<

%.tqd: %.mwx
	emusim.x --capture_timing_queues -- $<

.PHONY: clean
clean:
	rm -f *.mwx *.tqd *.cdc *.vsf
