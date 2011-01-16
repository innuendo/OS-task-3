/*
 *  strategy_lfu.c
 *
 */

#include <stdint.h>

#include "page.h"
#include "strategy.h"

void update_strategy_metadata(unsigned page_num) {}
void init_strategy_metadata() {}

page * select_page(page * pages, size_t size)
{
	printf("select_page! size = %d\n", size);
	page * current_min;
	uint64_t min = pages[0].meta->reference_counter;
	for (int i = 0; i < size; ++i) {
		printf("SELECT_PAGE: i = %d\n", i);
		if (pages[i].meta->reference_counter < min) {
			min = pages[i].meta->reference_counter;
			current_min = &pages[i];
		}
	}
	return current_min;
}
