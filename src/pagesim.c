#include <sys/types/h>
#include "page.h"
#include "strategy.h"
#include "pagesim.h"
#include "common.h"
#include "err.h"
#include "frame.h"
static struct
{
    page pages[512];
    frame * frames[64];
    bool active;
    unsigned mem_size;
    unsigned addr_space_size;
    unsigned max_concurrent_operations;
    pagesim_callback logger;
} globals;

int page_sim_init(unsigned page_size, 
                  unsigned mem_size,
                  unsigned addr_space_size,
                  unsigned max_concurrent_operations,
                  pagesim_callback callback) 
{
    globals.mem_size = mem_size;
    globals.addr_space_size = addr_space_size;
    globals.max_concurrent_operations = max_concurrent_operations;
    globals.logger = callback;
    for (int i = 0; i < mem_size_; ++i) 
    {
        page[i] = i;
        frame[i] = calloc(page_size, sizeof(uint8_t));
    }
    globals.active = true;
    /*disk initialization*/
}

int page_sim_get(unsigned a, uint8_t *v)
{
    if (globals.active)
    {
        if (pages[a].correct) 
        {
            
        }
        else 
        {
            /*fetch from "DISK"*/
        }
    }
    else
    {
        
    }
}
int page_sim_set(unsigned a, uint8_t v)
{
    if (globals.active)
    {
        
    }
    else
    {
        
    }
}

int page_sim_end() 
{
    globals.active = false;
    for (i = 0; i < globals.mem_size; ++i) 
    {
        free(frame[i]);
    }
}