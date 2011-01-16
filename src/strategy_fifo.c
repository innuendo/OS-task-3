/*
 *  strategy_fifo.c
 *  Z3
 *
 *  Created by Krzysztof Wiśniewski on 11-01-11.
 *
 */

#include <pthread.h>

#include "common.h"
#include "strategy.h"

typedef struct queue64
{
	unsigned container[65];
	unsigned front;
	unsigned tail;
} queue64;

static queue64 current_pages;

static inline void queue64_init(queue64 q)
{
	q.tail = 0;
	q.front = 0;
}

static inline bool queue64_empty(queue64 q)
{
	return (q.front = q.tail) ? true : false;
}

static inline void queue64_push_back(queue64 q, unsigned x)
{
	q.container[q.tail] = x;
	if (++q.tail > 64)
		q.tail = 0;
}

static inline unsigned queue64_front(queue64 q)
{
	return q.container[q.front];
}

static inline void queue64_pop_front(queue64 q)
{
	if (++q.front > 64)
		q.tail = 0;
}

void init_strategy_metadata()
{
	queue64_init(current_pages);
	for (int i = 0; i < 64; ++i)
	{
		queue64_push_back(current_pages, i);
	}
}

void update_strategy_metadata(unsigned page_num)
{
	queue64_pop_front(current_pages);
	queue64_push_back(current_pages, page_num);
}

page * select_page(page * pages, size_t size)
{
	return (pages + current_pages.front);
}
