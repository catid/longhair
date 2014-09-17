# Change your compiler settings here

# Clang seems to produce faster code
#CCPP = g++
#CC = gcc
#OPTFLAGS = -O3 -fomit-frame-pointer -funroll-loops
CCPP = clang++ -m64
CC = clang -m64
OPTFLAGS = -O4
DBGFLAGS = -g -O0 -DDEBUG
CFLAGS = -Wall -fstrict-aliasing
OPTLIBNAME = liblonghair.a


# Object files

library_o = MemXOR.o MemSwap.o cauchy_256.o


# Release target (default)

release : CFLAGS += $(OPTFLAGS)
release : LIBNAME = $(OPTLIBNAME)
release : library


# Library target

library : clean $(library_o)
	ar rcs $(LIBNAME) $(library_o)


# LibCat objects

MemXOR.o : MemXOR.cpp
	$(CCPP) $(CFLAGS) -c MemXOR.cpp

MemSwap.o : MemSwap.cpp
	$(CCPP) $(CFLAGS) -c MemSwap.cpp


# Library objects

cauchy_256.o : cauchy_256.cpp
	$(CCPP) $(CFLAGS) -c cauchy_256.cpp


# Cleanup

.PHONY : clean

clean :
	git submodule update --init
	-rm *.a *.o

