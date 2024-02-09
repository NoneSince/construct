CXX = g++
CXXFLAGS = -std=c++11 -Wall --pedantic-errors -g
SDIR = src
EDIR = examples
BDIR = bin
ODIR = out
_OBJS = construct_debug.o construct_flags.o deconstruct.o reconstruct.o construct.o
OBJS =  $(patsubst %,$(BDIR)/%,$(_OBJS))
PROG = construct.exe

.PHONY: all clean test

all: $(OBJS) $(BDIR)/$(PROG)

$(BDIR)/$(PROG): $(OBJS)
	mkdir -p $(BDIR)
	$(CXX) $(OBJS) -o $(BDIR)/$(PROG) $(CXXFLAGS)

$(BDIR)/construct.o: $(SDIR)/construct.cpp $(SDIR)/deconstruct.h $(SDIR)/reconstruct.h $(SDIR)/construct_flags.h $(SDIR)/construct_types.h
	mkdir -p $(BDIR)
	$(CXX) -c $(SDIR)/construct.cpp -o $(BDIR)/construct.o $(CXXFLAGS)

$(BDIR)/construct_debug.o: $(SDIR)/construct_debug.cpp $(SDIR)/construct_debug.h $(SDIR)/construct_types.h $(SDIR)/reconstruct.h
	mkdir -p $(BDIR)
	$(CXX) -c $(SDIR)/construct_debug.cpp -o $(BDIR)/construct_debug.o $(CXXFLAGS)

$(BDIR)/construct_flags.o: $(SDIR)/construct_flags.cpp $(SDIR)/construct_flags.h $(SDIR)/construct_types.h
	mkdir -p $(BDIR)
	$(CXX) -c $(SDIR)/construct_flags.cpp -o $(BDIR)/construct_flags.o $(CXXFLAGS)

$(BDIR)/deconstruct.o: $(SDIR)/deconstruct.cpp $(SDIR)/deconstruct.h $(SDIR)/construct_types.h
	mkdir -p $(BDIR)
	$(CXX) -c $(SDIR)/deconstruct.cpp -o $(BDIR)/deconstruct.o $(CXXFLAGS)

$(BDIR)/reconstruct.o: $(SDIR)/reconstruct.cpp $(SDIR)/reconstruct.h $(SDIR)/construct_types.h
	mkdir -p $(BDIR)
	$(CXX) -c $(SDIR)/reconstruct.cpp -o $(BDIR)/reconstruct.o $(CXXFLAGS)

clean:
	rm -rf $(BDIR) $(ODIR)

test: $(BDIR)/$(PROG)
	rm -rf $(ODIR)
	mkdir -p $(ODIR)
	$(BDIR)/$(PROG) -f elf64 -i $(EDIR)/factorial.con -o $(ODIR)/factorial.asm
	diff --strip-trailing-cr $(EDIR)/factorial.asm $(ODIR)/factorial.asm
	$(BDIR)/$(PROG) -f elf64 -i $(EDIR)/strchr.con    -o $(ODIR)/strchr.asm
	diff --strip-trailing-cr $(EDIR)/strchr.asm    $(ODIR)/strchr.asm
	$(BDIR)/$(PROG) -f elf64 -i $(EDIR)/strlwr.con    -o $(ODIR)/strlwr.asm
	diff --strip-trailing-cr $(EDIR)/strlwr.asm    $(ODIR)/strlwr.asm
