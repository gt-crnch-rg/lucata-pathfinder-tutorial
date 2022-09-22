CC=gcc
CFLAGS=-g -Wall -fPIE

LGBLIB=libgraphblas.so

CPPFLAGS=-I.
LDLIBS=${LGBLIB} -lm

.PHONY:	all

all:	main

main:	main.o bfs.o pagerank.o degree_pseudoinv.o scale_vector.o read_dumped.o

mmio/mmio:	mmio/example_read.c
	cd mmio && gcc -I.. -g -o mmio example_read.c mmio.c

main.o:	main.c
bfs.o:	bfs.c
pagerank.o:	pagerank.c
degree_pseudoinv.o:	degree_pseudoinv.c
scale_vector.o:	scale_vector.c
read_dumped.o:	read_dumped.c

.PHONY:	clean
clean:
	rm main main.o bfs.o pagerank.o degree_pseudoinv.o scale_vector.o read_dumped.o
