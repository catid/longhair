CC	= gcc
CFLAGS = -O4
OBJS = galois.o jerasure.o cauchy.o cauchy_03.o

tester: $(OBJS)
	$(CC) $(OBJS) -o tester

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c $<

galois.o: galois.h
jerasure.o: jerasure.h galois.h
cauchy.o: jerasure.h galois.h cauchy.h
cauchy_03.o: jerasure.h galois.h cauchy.h

clean:
	rm tester *.o

.PHONY: all headers

