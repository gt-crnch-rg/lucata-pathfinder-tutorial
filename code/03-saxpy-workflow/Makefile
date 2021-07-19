#Makefile for Notebook #3

EMU_BASE=/tools/emu/pathfinder-sw/21.06
CFLAGS=-I ${EMU_BASE}/x86/include/emu_c_utils -I ${EMU_BASE}/include -fcilkplus -DX86 
LDFLAGS=-L ${EMU_BASE}/x86/lib -l emu_c_utils -o saxpy-1d-workflow-x86

all: saxpy-1d-workflow.mwx saxpy-1d-workflow-x86

.PHONY: sim
sim: saxpy-1d-workflow.tqd

saxpy-1d-workflow-x86: saxpy-1d-workflow.c
	gcc-7 -o $@ $< $(CFLAGS) $(LDFLAGS)

saxpy-1d-workflow.mwx: saxpy-1d-workflow.c
	emu-cc -o $@ $<

.PHONY: clean
clean:
	rm -f *.mwx *-x86 *.tqd *.cdc *.vsf *.mps *.hdd
