# Change your compiler settings here

# Clang seems to produce faster code
#CCPP = g++
#CC = gcc
#OPTFLAGS = -O3 -fomit-frame-pointer -funroll-loops
CCPP = clang++ -m64
CC = clang -m64
OPTFLAGS = -O4
DBGFLAGS = -g -O0 -DDEBUG
CFLAGS = -Wall -fstrict-aliasing -I./libcat -I./include
OPTLIBNAME = bin/liblonghair.a
DBGLIBNAME = bin/liblonghair_debug.a


# Object files

library_o = cauchy_256.o MemXOR.o MemSwap.o BitMath.o

test_o = cauchy_256_tests.o Clock.o


# Release target (default)

release : CFLAGS += $(OPTFLAGS)
release : LIBNAME = $(OPTLIBNAME)
release : library


# Debug target

debug : CFLAGS += $(DBGFLAGS) -DCAT_CAUCHY_LOG
debug : LIBNAME = $(DBGLIBNAME)
debug : library


# Library target

library : clean $(library_o)
	ar rcs $(LIBNAME) $(library_o)


# test executable

valgrind : CFLAGS += -DUNIT_TEST $(DBGFLAGS)
valgrind : debug $(test_o)
	$(CCPP) $(test_o) -L./bin -llonghair_debug -o test
	valgrind --dsymutil=yes ./test

test : CFLAGS += -DUNIT_TEST $(OPTFLAGS)
test : release $(test_o)
	$(CCPP) $(test_o) -L./bin -llonghair -o test
	./test

test-mobile : CFLAGS += -DUNIT_TEST $(OPTFLAGS)
test-mobile : release $(test_o)
	$(CCPP) $(test_o) -L./longhair-mobile -llonghair -o test
	./test


# LibCat objects

Clock.o : libcat/Clock.cpp
	$(CCPP) $(CFLAGS) -c libcat/Clock.cpp

BitMath.o : libcat/BitMath.cpp
	$(CCPP) $(CFLAGS) -c libcat/BitMath.cpp

MemXOR.o : libcat/MemXOR.cpp
	$(CCPP) $(CFLAGS) -c libcat/MemXOR.cpp

MemSwap.o : libcat/MemSwap.cpp
	$(CCPP) $(CFLAGS) -c libcat/MemSwap.cpp


# Library objects

cauchy_256.o : src/cauchy_256.cpp
	$(CCPP) $(CFLAGS) -c src/cauchy_256.cpp


# Executable objects

cauchy_256_tests.o : tests/cauchy_256_tests.cpp
	$(CCPP) $(CFLAGS) -c tests/cauchy_256_tests.cpp


# Cleanup

.PHONY : clean

clean :
	git submodule update --init
	-rm bin/*.a test *.o

