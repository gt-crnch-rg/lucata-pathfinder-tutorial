#Makefile for Notebook #4

LDFLAGS=-l emu_c_utils

all: intrs-hooks-saxpy.mwx

.PHONY: sim
sim: saxpy.tqd

%.mwx: %.c
	emu-cc -o $@ $<  $(LDFLAGS)

%.tqd: %.mwx
	emusim.x --capture_timing_queues -- $<

.PHONY: clean
clean:
	rm -rf *.mwx *.tqd *.cdc *.vsf *.mps *_profile
