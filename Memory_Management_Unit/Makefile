all: process.c MMU.c master.c sched.c
	gcc master.c -o master -lm
	gcc sched.c -o sched -lm
	gcc MMU.c -o MMU -lm
	gcc process.c -o process -lm

clean:
	rm process master sched MMU