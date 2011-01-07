#ifndef _FRAME_H
#define _FRAME_H

#include <pthread.h>

typedef struct frame 
{
    pthread_mutex_t _mutex;
    uint8_t* _frame;
} frame;

#endif