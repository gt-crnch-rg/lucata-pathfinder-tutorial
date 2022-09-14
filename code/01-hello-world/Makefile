#Sample Makefile to demonstrate how to compile the Emu examples

#Uncomment only one of these lines
#Use the last stable toolchain
#TOOLCHAIN=/tools/lucata/pathfinder-sw/current
#Use the latest beta toolchain
TOOLCHAIN=/tools/lucata/pathfinder-sw/22.09-beta

INCLUDES=${TOOLCHAIN}/include/ 
LIBPATH=${TOOLCHAIN}/lib
LIBRARIES=-lmemoryweb

all: hello-world-naive.mwx hello-world.mwx hello-world-spawn.mwx hello-world-spawn-at.mwx

.PHONY: sim
sim: hello-world-naive.tqd hello-world.tqd hello-world-spawn.tqd hello-world-spawn-at.tqd

%.mwx: %.c
	emu-cc -o $@ -I${INCLUDES} -L${LIBPATH} ${LIBRARIES} $<

%.tqd: %.mwx
	emusim.x --capture_timing_queues --total_nodes 2 -m 24 -- $<

test:  hello-world-naive.mwx
	emusim.x --capture_timing_queues --total_nodes 2 -m 24 -- $<

.PHONY: clean
clean:
	rm -f *.mwx *.tqd *.cdc *.vsf *.mps; \
	./helpers/backup_imgs.sh


#Remove all non-source files
reallyclean:
	rm -rf *.mwx *.tqd *.cdc *.vsf *.mps figs
