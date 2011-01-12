#ifndef _PAGE_H
#define _PAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "common.h"

typedef struct page {
    uint64_t reference_counter;
    uint64_t number;
    uint8_t frame_number;
    int page_index;
    bool modified;
    bool correct;
    pthread_mutex_t lock;
} page;

#endif