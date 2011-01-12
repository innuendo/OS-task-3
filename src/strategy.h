#ifndef _STRATEGY_H
#define _STRATEGY_H

#include "page.h"

page * select_page(page * pages, size_t size);
void update_metadata(unsigned page_num);
void init_metadata();
#endif
