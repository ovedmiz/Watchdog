CC = gcc
FLAGS = -ansi -pedantic-errors -Wall -Wextra -g 

all: libwd.so process_wd run

libwd.so: wd.c shared_wd.c
	$(CC) $(FLAGS) -fPIC -shared wd.c shared_wd.c wd_test.c ../../ds/src/sched/sched.c ../../ds/src/event/event.c ../../ds/src/uid/uid.c ../../ds/src/pq/pq.c ../../ds/src/sorted_list/sorted_list.c ../../ds/src/d_list/d_list.c -I../../ds/src/sched -lpthread -o libwd.so

heap:
	 $(CC) $(FLAGS) wd.c shared_wd.c wd_test.c -I../../../ds/sched ../../../ds/src/sched/sched.c ../../../ds/src/sched/event.c ../../../ds/src/uid/uid.c ../../../ds/src/pq_heap/pq_heap.c ../../../ds/src/vector/vector.c ../../../ds/src/heapify/heapify.c  -lpthread

process_wd:
	$(CC) $(FLAGS) process_wd.c shared_wd.c ../../ds/src/sched/sched.c ../../ds/src/event/event.c ../../ds/src/uid/uid.c ../../ds/src/pq/pq.c ../../ds/src/sorted_list/sorted_list.c ../../ds/src/d_list/d_list.c -I../../ds/src/sched -lpthread -o process_wd.out
	
run: libwd.so process_wd
	$(CC) $(FLAGS) wd_test.c -L. -Wl,-rpath=. -I../../header -lwd -o wd.out
	
clean:
	rm -f *.so
	rm -f process_wd
	rm -f *~
	rm *.out
