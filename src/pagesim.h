#ifndef _PAGESIM_H
#define _PAGESIM_H

#include <stdint.h>

typedef void (*pagesim_callback)(int op, int arg1, int arg2);

extern int page_sim_init(unsigned page_size, unsigned mem_size,
                         unsigned addr_space_size, unsigned max_concurrent_operations,
                         pagesim_callback callback);

extern int page_sim_get(unsigned a, uint8_t *v);
extern int page_sim_set(unsigned a, uint8_t v);

extern int page_sim_end();

#endif
