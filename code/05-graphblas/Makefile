#Sample Makefile to demonstrate how to compile the Emu examples

INCLUDES=/tools/lucata/pathfinder-sw/current/include/ 
LIBPATH=/tools/lucata/pathfinder-sw/current/lib
LIBRARIES=-lmemoryweb

all: hello-world-naive.mwx hello-world.mwx hello-world-spawn.mwx hello-world-spawn-at.mwx

.PHONY: sim
sim: hello-world-naive.tqd hello-world.tqd hello-world-spawn.tqd hello-world-spawn-at.tqd

%.mwx: %.c
	emu-cc -o $@ -I${INCLUDES} -L${LIBPATH} ${LIBRARIES} $<

%.tqd: %.mwx
	emusim.x --capture_timing_queues --total_nodes 2 -m 21 -- $<

test:  hello-world-naive.mwx
	emusim.x --capture_timing_queues --total_nodes 1 -m 21 -- $<

.PHONY: clean
clean:
	rm -f *.mwx *.tqd *.cdc *.vsf *.mps; \
	./helpers/backup_imgs.sh


#Remove all non-source files
reallyclean:
	rm -rf *.mwx *.tqd *.cdc *.vsf *.mps figs
