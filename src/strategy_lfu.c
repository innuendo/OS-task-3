/*
 *  strategy_lfu.c
 *
 */

#include "strategy.h"

page* select_page(page* pages, size_t size)
{
	uint64_t min = pages->reference_counter; 
	page result = pages[0];
	for (size_t i = 0; i < size; ++i)
	{
		if ((pages + i)->reference_counter < min)
		{
			result = &pages[i];
			min = (pages + i)->reference_counter;
		}
	}
	return result;
}
