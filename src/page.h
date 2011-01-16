#ifndef _PAGE_H
#define _PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "common.h"

typedef struct page_meta {
    uint64_t reference_counter;
    int16_t frame_number;
    int page_index;
    bool modified;
    pthread_mutex_t lock;
} page_meta;

typedef struct page {
	uint8_t * data;
	page_meta * meta;
} page;

#endif