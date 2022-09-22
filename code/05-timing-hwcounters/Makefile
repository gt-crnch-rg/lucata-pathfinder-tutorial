#Makefile for Notebook #4

#LUCATA_BASE gets set by the global .env file in the directory above
INCLUDES=${LUCATA_BASE}/include/memoryweb 
LIBPATH=${LUCATA_BASE}/lib
LIBRARIES=-lemu_c_utils -lmemoryweb

all: timing-hooks-saxpy.mwx

.PHONY: sim
sim: timing-hooks-saxpy.tqd

%.mwx: %.c
	emu-cc -o $@ -I${INCLUDES} -L${LIBPATH} ${LIBRARIES} $<

%.tqd: %.mwx
	emusim.x --capture_timing_queues --total_nodes 2 -m 24 -- $<

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
