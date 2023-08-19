all:
	../../build/aria main.ar
	as -g -o _start.o _start.S
	ld mod.o _start.o

clean:
	rm *.o a.out
