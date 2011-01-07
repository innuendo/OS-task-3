#ifndef _PAGE_H
#define _PAGE_H

#include "common.h"

typedef struct page {
    int reference_counter;
    uint8_t frame_number;
    bool modified;
    bool correct;
} page;

#endif