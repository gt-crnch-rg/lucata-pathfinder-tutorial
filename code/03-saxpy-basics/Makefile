#Makefile for Notebook #2

all: saxpy.mwx saxpy-for.mwx saxpy-1d.mwx saxpy-1d-hint.mwx saxpy-2d-spawn-at.mwx saxpy-local-spawn-at.mwx saxpy-1d-replicated.mwx 

.PHONY: sim
sim: saxpy.tqd

%.mwx: %.c
	emu-cc -o $@ $<

%.tqd: %.mwx
	emusim.x --capture_timing_queues -- $<

.PHONY: clean
clean:
	rm -f *.mwx *.tqd *.cdc *.vsf *.mps
