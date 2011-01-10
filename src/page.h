#ifndef _PAGE_H
#define _PAGE_H

#include "pthread.h"
#include "common.h"

typedef struct page {
    int reference_counter; /*if negative then end of pages*/
    uint8_t frame_number;
	int page_index;
    bool modified;
    bool correct;
    pthread_mutex_t mutex;
} page;

#endif