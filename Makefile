CC=clang
CFLAGS=`llvm-config --cflags`
LD=clang++
LDFLAGS=`llvm-config --libs --cflags --ldflags core analysis executionengine interpreter native`

brainfuck: brainfuck.o
	$(LD) brainfuck.o $(LDFLAGS) -o brainfuck_cc

brainfuck.o: brainfuck.c
	$(CC) $(CFLAGS) -c brainfuck.c

OPT=-O3

compile:
	opt $(OPT) brainfuck.bc -o /tmp/new.bc && llc /tmp/new.bc > /tmp/new.s && clang /tmp/new.s $(OPT) -o bf_cc_output
