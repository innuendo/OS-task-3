CC=gcc
AR=ar
CFLAGS=-Wall -std=gnu99 -pthread

fifo: pagesim.o strategy_fifo.o
	$(AR) rcs libpagesim.a pagesim.o strategy_fifo.o

lfu: pagesim.o strategy_lfu.o
	$(AR) rcs libpagesim.a pagesim.o strategy_lfu.o

pagesim.o: pagesim.c strategy.h page.h
	$(CC) $(CFLAGS) -c pagesim.c -o pagesim.o

strategy_fifo.o: strategy_fifo.c strategy.h page.h
	$(CC) $(CFLAGS) -c strategy_fifo.c -o strategy_fifo.o
	
strategy_lfu.o: strategy_lfu.c strategy.h page.h
	$(CC) $(CFLAGS) -c strategy_lfu.c -o strategy_lfu.o

clean:
	rm -f *.o *.a
