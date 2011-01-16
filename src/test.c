#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include "pagesim.h"

#define THREAD_COUNT 4

#define PAGE_SIZE    4
#define PAGE_COUNT   8
#define FRAME_COUNT  2
#define IO_LIMIT     4
#define LOOP_ITERS   100

uint8_t daj_liczbe(unsigned adres) {
	return ((uint64_t) adres * 27011 + 2) % 251;
}

int licznik;

void my_callback(int op, int page, int frame) {
		//return;
	/*
	switch(op) {
		case 1:
			printf("Dostęp do strony %d\n", page);
			break;
			
		case 2:
			printf("Inicjuję zapis strony %d z ramki %d\n", page, frame);
			break;
			
		case 3:
			printf("Kończę zapis strony %d z ramki %d\n", page, frame);
			break;
			
		case 4:
			printf("Inicjuję wczytanie strony %d do ramki %d\n", page, frame);
			break;
			
		case 5:
			printf("Kończę wczytanie strony %d do ramki %d\n", page, frame);
			break;
			
		case 6:
			printf("Kończę dostęp do strony %d w ramce %d\n", page, frame);
			break;
	}*/
}

void *routine(void *data) {
	uint8_t liczba;
	int rnd;
	unsigned a;
		//  printf("Start wątku\n");
	for (int i = 0; i < LOOP_ITERS; ++i) {
		rnd = rand();
		a = (rnd >> 1) % (PAGE_SIZE * PAGE_COUNT);
		if(rnd & 1) {
			
			assert(page_sim_set(a, daj_liczbe(a)) == 0);
		}
		else {
			
			assert(page_sim_get(a, &liczba) == 0);
			assert(liczba == daj_liczbe(a));
		}
		
		licznik++;
		printf("i = %d \t", i);
		
	}
	return (void*) 1;
}

int main() {
	pthread_t threads[THREAD_COUNT];
	size_t i;
	unsigned a;
	uint8_t b;
	licznik = 0;
	setvbuf(stdout, 0, _IONBF, 0);
	assert(page_sim_init(PAGE_SIZE, FRAME_COUNT, PAGE_COUNT, IO_LIMIT, &my_callback) == 0);
	page_sim_set(1, 123);
	page_sim_get(1, &b);
	printf("%d\n", b);
	for(a = 0; a < PAGE_SIZE * PAGE_COUNT; a++) {
			//    printf("A[%d] := %d\n", a, (int) daj_liczbe(a));
		assert(page_sim_set(a, daj_liczbe(a)) == 0);
	}
	srand(time(NULL));
	
	for(i = 0; i < THREAD_COUNT; i++)
		assert(pthread_create(&threads[i], NULL, routine, NULL) == 0);
	
	for(i = 0; i < THREAD_COUNT; i++)
		assert(pthread_join(threads[i], NULL) == 0);
	assert(page_sim_end() == 0);
}