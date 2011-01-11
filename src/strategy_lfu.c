/*
 *  strategy_lfu.c
 *
 */

#include <pthreads.h>

#include "common.h"
#include "strategy.h"

void update_metadata(unsigned page_num)
{
}
void init_metadata()
{
}

page * select_page(page * pages, size_t size)
{
	uint64_t min = pages->reference_counter;
	page *result = pages, *prev = NULL;
	if (pthread_mutex_lock(&result->lock) != RETURN_SUCCESS)
		return NULL;
	result->correct = false;
	for (size_t i = 1; i < size; ++i)
	{
		if ((pages + i)->correct)
		{
			if (pthread_mutex_lock(&(pages + i)->lock) != RETURN_SUCCESS)
				return NULL;
			if ((pages + i)->reference_counter < min)
			{
				prev = result;
				result = (pages + i);
				min = (pages + i)->reference_counter;
				prev->correct = true;
				if (pthread_mutex_unlock(&prev->lock) != RETURN_SUCCESS)
					return NULL;
			}
			else
			{
				if (pthread_mutex_unlock(&(pages + i)->lock)
					!= RETURN_SUCCESS)
					return NULL;
			}
		}
	}
	return result;
}
