#ifndef _COMMON_H
#define _COMMON_H

enum bool { false, true };

extern void * calloc_wrapper(size_t number, size_t size);
extern void * malloc_wrapper(size_t size) ;

#endif