#Sample Makefile to demonstrate how to compile the Emu examples

INCLUDES=/tools/lucata/pathfinder-sw/22.02/include/memoryweb/ 
LIBPATH=/tools/lucata/pathfinder-sw/22.02/lib
LIBRARIES=-lmemoryweb

all: hello-world-naive.mwx hello-world.mwx hello-world-spawn.mwx hello-world-spawn-at.mwx

.PHONY: sim
sim: hello-world-naive.tqd hello-world.tqd hello-world-spawn.tqd hello-world-spawn-at.tqd

%.mwx: %.c
	emu-cc -o $@ -I${INCLUDES} -L${LIBPATH} ${LIBRARIES} $<

%.tqd: %.mwx
	emusim.x --capture_timing_queues --total_nodes 2 -m 20 -- $<

test:  hello-world-naive.mwx
	emusim.x --capture_timing_queues --total_nodes 1 -m 20 -- $<

.PHONY: clean
clean:
	rm -f *.mwx *.tqd *.cdc *.vsf *.mps; \
	mkdir -p figs && mv *.png figs/.
