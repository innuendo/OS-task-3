#include <sys/types.h>
#include <stdlib.h>
#include "common.h"
#include "err.h"

void * calloc_wrapper(size_t number, size_t size) 
{   
    void * ptr;
    if ((ptr = calloc(number, size)) == NULL)
        syserr("calloc failed!");
    return ptr;
}

void * malloc_wrapper(size_t size) 
{   
    void * ptr;
    if ((ptr = malloc(size)) == NULL) 
        syserr("malloc failed!");
    return ptr;
}