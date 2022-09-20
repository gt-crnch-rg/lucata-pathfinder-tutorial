#Sample Makefile to demonstrate how to compile the Emu examples

#LUCATA_BASE gets set by the global .env file in the directory above
INCLUDES=${LUCATA_BASE}/include/ 
LIBPATH=${LUCATA_BASE}/lib
LIBRARIES=-lmemoryweb

#You can change this string to plot different applications
TESTSRC=hello-world

all: check-env hello-world-naive.mwx hello-world.mwx hello-world-spawn.mwx hello-world-spawn-at.mwx

.PHONY: sim
sim: hello-world-naive.tqd hello-world.tqd hello-world-spawn.tqd hello-world-spawn-at.tqd

profile: ${TESTSRC}.mwx
	emusim_profile profile_${TESTSRC} --total_nodes 2 -m 24 -- $<

%.mwx: %.c
	emu-cc -o $@ -I${INCLUDES} -L${LIBPATH} ${LIBRARIES} $<

test:  hello-world-naive.mwx
	emusim.x --total_nodes 2 -m 24 -- $<

.PHONY: clean
clean:
	rm -f *.mwx *.tqd *.cdc *.vsf *.mps *.uis *.csv *.hpc; \
	./helpers/backup_imgs.sh

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
