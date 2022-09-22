#Makefile for Notebook #3

#LUCATA_BASE gets set by the global .env file in the directory above
INCLUDES=${LUCATA_BASE}/include/memoryweb 
LIBPATH=${LUCATA_BASE}/lib
LIBRARIES=-lmemoryweb

#Flags and compiler used to compile x86 versions of the code
X86CC=/tools/lucata/opencilk-2.0.1/bin/clang
X86FLAGS=-I/tools/lucata/pathfinder-sw/22.09-beta/include/cilk/ -I/tools/emu/pathfinder-sw/22.09-beta/x86/include/emu_c_utils /tools/emu/pathfinder-sw/22.09-beta/x86/lib/libemu_c_utils.a -DX86

all: saxpy-1d-workflow-x86 saxpy-1d-workflow.mwx

.PHONY: sim
sim: saxpy-1d-workflow.tqd

saxpy-1d-workflow-x86: saxpy-1d-workflow.c
	${X86CC} ${X86FLAGS} -o $@ $< 

test-x86: saxpy-1d-workflow-x86
	CILK_NWORKERS=2 ./$< 8 1048576 5

%.mwx: %.c
	emu-cc -o $@ -I${INCLUDES} -L${LIBPATH} ${LIBRARIES} $<

.PHONY: clean
clean:
	rm -f *.mwx *-x86 *.tqd *.cdc *.mps *.hdd *.uis *.csv *.hpc

#Check to see if the right toolchain was set
check-env:
ifeq ($(LUCATA_BASE),)
	@echo "No toolchain found! Remember to source the .env file with 'source ../.env"
else
	@echo "Using Lucata toolchain at $(LUCATA_BASE)."
endif

#Remove all non-source files
reallyclean:
	rm -rf *.mwx *.tqd *.cdc *.vsf *.mps *.uis *.csv *.hpc figs profile_${TESTSRC}/*;
