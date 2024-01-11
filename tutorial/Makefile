# Standard makefile
BASE = /usr/local/lucata
BIN = $(BASE)/bin
EXE ?= vectoradd$(N)
LDFLAGS += -l memoryweb
SFLAGS += -m 33 --total_nodes 8
XCFLAGS = -I $(BASE)/x86/include/emu_c_utils -DX86
XLFLAGS = -l emu_c_utils -L $(BASE)/x86/lib
MS = 1

.PRECIOUS: %.prof.mwx

runmwx: $(EXE).mwx
	emusim.x $(SFLAGS) -- $< $(IN)

runx86: $(EXE).x86
	./$< $(IN)

runprof: $(EXE).prof.mwx
	emusim.x $(SFLAGS) -- $< $(IN)

runhw: $(EXE).mwx
	emu_multinode_exec 0 -- $< $(IN)

runpc: $(EXE).prof.mwx
	emu_multinode_exec 0 -- $< $(IN)

runpr: $(EXE).prof.mwx
	emu_multinode_exec 0 --poll_reg $(MS) -- $< $(IN)

pcplots: mn_exec_pc.$(PID).hpc
	make_hpc_plots.py -f $<

mn_exec_pc.%.hpc: mn_exec_pc.%.log
	emu_pc_concat.py -f $<

prplots: mn_exec_pr.$(PID).log
	emu_analyze_pr.py -f $<

intrs_hook.x86: intrs_hook.c
	gcc $(XCFLAGS) -o $@ $^ $(XLFLAGS)

%.x86: %.c
	gcc $(XCFLAGS) -o $@ $^

vectoradd.mwx vectoradd1.mwx: LDFLAGS=
intrs_hook.mwx: LDFLAGS=-l emu_c_utils -l memoryweb

%.mwx: %.c
	emu-cc $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.prof.mwx: %.c
	emu-cc -DLUPROFILE $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.profdir: %.prof.mwx
	emusim_profile $@ $(SFLAGS) -- $< $(IN)

%.st: %.prof.mwx
	emusim.x $(SFLAGS) --short_trace -- $< $(IN) > $@

%.visa: %.prof.mwx
	emusim.x $(SFLAGS) --verbose_isa -- $< $(IN) > $@

%.uis: %.prof.mwx
	emusim.x $(SFLAGS) --output_instruction_count -- $< $(IN)

%.tqd: %.prof.mwx
	emusim.x $(SFLAGS) --capture_timing_queues -- $< $(IN)

%.mt: %.prof.mwx
	emusim.x $(SFLAGS) --memory_trace -- $< $(IN)

%.tsv: %.prof.mwx
	emusim.x $(SFLAGS) --visit_history -- $< $(IN)

%.tpc: %.prof.mwx
	emusim.x $(SFLAGS) --track_tpcs -- $< $(IN)

%.s: %.c
	emu-cc -S $(CFLAGS) $<

%.od: %.mwx
	gossamer64-objdump -Dx $< > $@

%.a:
	gossamer64-ar -rv $@ $^
	gossamer64-ranlib $@

clean:
	rm -f *.o *.s *.a *.mwx *.x86 *.cdc *.hpc *.mps *.st *.visa *.uis *.tqd *.mt *.tpc *.tsv *.od
