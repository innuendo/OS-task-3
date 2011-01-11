/*
 *  strategy_fifo.c
 *  Z3
 *
 *  Created by Krzysztof Wiśniewski on 11-01-11.
 *
 */

#include <pthreads.h>

#include "common.h"
#include "strategy.h"

static typedef struct queue64
{
	unsigned container[65];
	unsigned front;
	unsigned tail;
} queue64;

static queue64 current_frames;

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
	(++q.tail > 64) q.tail = 0;
}

static inline unsigned queue64_front(queue64 q)
{
	return q.container[q.front];
}

static inline void queue64_pop_front(queue64 q)
{
	(++q.front > 64) ? q.tail = 0;
}

void init_metadata()
{
	queue64_init(current_frames);
	for (int i = 0; i < 64; ++) {
		queue64_push_back(current_frames, i);
	}
}

void update_metadata(unsigned page_num)
{
	queue64_pop_front(current_frames);
	queue64_push_back(current_frames, page_num);
}

page * select_page(page * pages, size_t size)
{
	return (pages + current_frames.front);
}