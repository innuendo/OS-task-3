/******************************************************************************
 * pagesim.c                                                                  *
 * Author: Krzysztof Wisniewski                                               *
 *                                                                            *
 *****************************************************************************/

/**************************** included headers *******************************/
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <aio.h>

#include <stdio.h>

#include "page.h"
#include "strategy.h"
#include "pagesim.h"
#include "common.h"
/*****************************************************************************/

/**************************** global variables *******************************/
static struct
{
    page pages[64];
	page_meta metadata[512];
    bool active;
    unsigned page_size;
    unsigned mem_size;
    unsigned addr_space_size;
    unsigned max_concurrent_operations;
    unsigned act_concurrent_operations;
	int swap_fd;
    pthread_cond_t io_oveflow_cond;
    pthread_mutex_t concurrent_operations_mutex;
    pthread_mutex_t logger_mutex;
	pthread_mutex_t select_mutex;
    pagesim_callback logger;
} globals;
/*****************************************************************************/

/**************************** interface **************************************/
int page_sim_init(unsigned page_size, unsigned mem_size,
                  unsigned addr_space_size, unsigned max_concurrent_operations,
                  pagesim_callback callback);
int page_sim_get(unsigned a, uint8_t *v);
int page_sim_set(unsigned a, uint8_t v);
int page_sim_end();
/*****************************************************************************/

/**************************** helpers ****************************************/
static int callback_wrapper(int op, int arg1, int arg2)
{
    if (pthread_mutex_lock(&globals.logger_mutex) == RETURN_ERROR)
        return RETURN_ERROR;
    globals.logger(op, arg1, arg2);
    if (pthread_mutex_unlock(&globals.logger_mutex) == RETURN_ERROR)
        return RETURN_ERROR;
	return RETURN_SUCCESS;
}

static inline void aiocb_setup(struct aiocb *aio_init,
							   unsigned page_index,
							   unsigned page_num)
{
    aio_init->aio_fildes = globals.swap_fd;
    aio_init->aio_offset = page_index * globals.page_size;
    aio_init->aio_nbytes = globals.page_size;
    aio_init->aio_buf = globals.pages[page_num].data;
    aio_init->aio_sigevent.sigev_notify = SIGEV_NONE;
	aio_init->aio_reqprio = 0;
}

static int init_pages()
{
    for (int i = 0; i < globals.addr_space_size; ++i)
    {
        if (pthread_mutex_init(&globals.metadata[i].lock, NULL) == RETURN_ERROR)
            return RETURN_ERROR;
        
        globals.metadata[i].page_index = i;
        globals.metadata[i].reference_counter = 0;
        globals.metadata[i].frame_number = -1;
        globals.metadata[i].modified = false;
    }
    for (int i = 0; i < globals.mem_size; ++i)
    {
        globals.pages[i].data = calloc(globals.page_size, sizeof(uint8_t));
        if (globals.pages[i].data == NULL)
            return RETURN_ERROR;
        globals.metadata[i].frame_number = i;
        globals.pages[i].meta = &globals.metadata[i];
    }
	struct aiocb * aio_init_table[1];
	struct aiocb aio_init;
	aio_init_table[0] = &aio_init;
	aiocb_setup(&aio_init, 0, 0);
	for (int i = 0; i < globals.addr_space_size; ++i) {
		if (aio_write(&aio_init) == RETURN_ERROR) return RETURN_ERROR;
		if (aio_suspend((const struct aiocb * const *)aio_init_table, 1, NULL) 
			== RETURN_ERROR)
			return RETURN_ERROR;
	}
    return RETURN_SUCCESS;
}

static inline int init_swap()
{
    if ((globals.swap_fd = open("swap", O_CREAT | O_RDWR))
        == RETURN_ERROR)
        return RETURN_ERROR;
    return RETURN_SUCCESS;
}

static int translate_address(unsigned a, unsigned *page_num, unsigned *offset)
{
    *page_num = a / globals.page_size;
    *offset = a % globals.page_size;
    if (*offset > globals.page_size || *page_num > globals.addr_space_size)
        return RETURN_ERROR;
    else
        return RETURN_SUCCESS;
}

static int increase_io_operations_counter()
{
    if (pthread_mutex_lock(&globals.concurrent_operations_mutex)
        == RETURN_ERROR)
        return RETURN_ERROR;
    while (globals.act_concurrent_operations
           > globals.max_concurrent_operations)
    {
        if (pthread_cond_wait(&globals.io_oveflow_cond,
                              &globals.concurrent_operations_mutex)
			== RETURN_ERROR)
        {
            --globals.act_concurrent_operations;
            return RETURN_ERROR;
        }
    }
	++globals.act_concurrent_operations;
    if (pthread_mutex_unlock(&globals.concurrent_operations_mutex)
        == RETURN_ERROR)
    {
        --globals.act_concurrent_operations;
        return RETURN_ERROR;
    }
    return RETURN_SUCCESS;
}

static int decrease_io_operations_counter()
{
    if (pthread_mutex_lock(&globals.concurrent_operations_mutex)
        == RETURN_ERROR)
    {
        --globals.act_concurrent_operations;
        return RETURN_ERROR;
    }
    --globals.act_concurrent_operations;
    if (pthread_mutex_unlock(&globals.concurrent_operations_mutex)
        == RETURN_ERROR)
    {
        --globals.act_concurrent_operations;
        return RETURN_ERROR;
    }
    if (pthread_cond_signal(&globals.io_oveflow_cond) != RETURN_SUCCESS)
        return RETURN_ERROR;
	return RETURN_SUCCESS;
}

static page * select_page_wrapper()
{
	page * current_page;
	if (pthread_mutex_lock(&globals.select_mutex) == RETURN_ERROR)
		return NULL;
	current_page = select_page(globals.pages, globals.mem_size);
	if (current_page == NULL)
	{
		pthread_mutex_unlock(&globals.select_mutex);
		return NULL;
	}
	if (pthread_mutex_lock(&(current_page->meta->lock)) == RETURN_ERROR)
	{
		pthread_mutex_unlock(&globals.select_mutex);
		return NULL;
	}
	return current_page;
}

static int write_to_swap(page_meta * swapped_meta)
{
	struct aiocb * aio_init_table[1];
	struct aiocb aio_init;
	aio_init_table[0] = &aio_init;
	aiocb_setup(&aio_init,
				swapped_meta->page_index,
				swapped_meta->frame_number);
	if (increase_io_operations_counter() == RETURN_ERROR)
		return RETURN_ERROR;
	
	if (swapped_meta->modified)
	{
		callback_wrapper(2, swapped_meta->page_index, 
						 swapped_meta->frame_number);
		if (aio_write(&aio_init) == RETURN_ERROR)
			return RETURN_ERROR;
		printf("aio_write ok\n");
		if (aio_suspend((const struct aiocb * const *)aio_init_table, 1, NULL)
			== RETURN_ERROR)
			return RETURN_ERROR;
		callback_wrapper(3, swapped_meta->page_index,
						 swapped_meta->frame_number);
	}
	
	swapped_meta->frame_number = -1;
	if (pthread_mutex_unlock(&swapped_meta->lock) != RETURN_SUCCESS)
		return RETURN_ERROR;
	return RETURN_SUCCESS;
}

static int read_from_swap(page * current_page,
						  unsigned page_num,
						  unsigned frame_num)
{
	struct aiocb * aio_init_table[1];
	struct aiocb aio_init;
	aio_init_table[0] = &aio_init;
	aiocb_setup(&aio_init, page_num, frame_num);
	
	callback_wrapper(4, page_num, frame_num);
	aio_read(&aio_init);
	aio_suspend((const struct aiocb * const *)aio_init_table, 1, NULL);
	callback_wrapper(5, page_num, frame_num);
	if (decrease_io_operations_counter() == RETURN_ERROR)
		return RETURN_ERROR;
	return RETURN_SUCCESS;
}

static page * get_page(unsigned page_num)
{
    page * current_page;
    unsigned frame_num;
    
    if (pthread_mutex_lock(&globals.metadata[page_num].lock) != RETURN_SUCCESS)
        return NULL;
	
    if (globals.metadata[page_num].frame_number == -1)
    {
        printf("GET_PAGE: page %d is not correct!\n", page_num);
		if ((current_page = select_page_wrapper()) == NULL)
			return NULL;
		
		/* already locked mutex to selected page */
        frame_num = current_page->meta->frame_number;
        update_strategy_metadata(page_num);
		
        printf("GET_PAGE: initialization ok!\n");
		page_meta * swapped_meta = current_page->meta;
		current_page->meta = &globals.metadata[page_num];
		if (pthread_mutex_unlock (&globals.select_mutex) == RETURN_ERROR)
			return NULL;
		if (write_to_swap(swapped_meta) == RETURN_ERROR) return NULL;
        if (read_from_swap(current_page, page_num, frame_num) == RETURN_ERROR)
			return NULL;
		
		current_page->meta->modified = false;
		current_page->meta->reference_counter = 0;
        current_page->meta->frame_number = frame_num;
    }
    else
		current_page = &globals.pages[globals.metadata[page_num].frame_number];
    current_page->meta->reference_counter++;
    return current_page;
}

static page * get_page_wrapper(unsigned a, unsigned *offset)
{
	if (!globals.active)
    {
        errno = ENOPROTOOPT;
        return NULL;
    }
	unsigned page_num;
    if (translate_address(a, &page_num, offset) == RETURN_ERROR)
    {
        errno = EFAULT;
        return NULL;
    }
	callback_wrapper(1, page_num, 0);
	return get_page(page_num);
}

static inline int finish_operation(page * current_page)
{
	callback_wrapper(6, current_page->meta->page_index, 
					 current_page->meta->frame_number);
	if (pthread_mutex_unlock(&current_page->meta->lock) == RETURN_ERROR)
        return RETURN_ERROR;
	return RETURN_SUCCESS;
}

static int check_init_constraints(unsigned page_size,
										 unsigned mem_size,
										 unsigned addr_space_size, 
										 unsigned max_concurrent_operations)
{
	if (!(BETWEEN(MIN_PAGE_SIZE, MAX_PAGE_SIZE, page_size))) return RETURN_ERROR;
	if (!(BETWEEN(MIN_MEM_SIZE, MAX_MEM_SIZE, mem_size))) return RETURN_ERROR;
	if (!(BETWEEN(MIN_ADDR_SPACE_SIZE, MAX_ADDR_SPACE_SIZE, addr_space_size)))
		return RETURN_ERROR;
	if (!(BETWEEN(MIN_MAX_CONCURRENT_OPERATIONS, MAX_MAX_CONCURRENT_OPERATIONS,
		max_concurrent_operations))) return RETURN_ERROR;
	return RETURN_SUCCESS;
}
/*****************************************************************************/

/**************************** core implementation ****************************/
int page_sim_init(unsigned page_size, unsigned mem_size,
                  unsigned addr_space_size, unsigned max_concurrent_operations,
                  pagesim_callback callback)
{
	if (check_init_constraints(page_size,
							   mem_size,
							   addr_space_size,
							   max_concurrent_operations) == RETURN_ERROR)
		return RETURN_ERROR;
    globals.page_size = page_size;
    globals.mem_size = mem_size;
    globals.addr_space_size = addr_space_size;
    globals.act_concurrent_operations = 0;
	globals.logger = callback;
	printf("\n\nglobals.page_size = %d; globals.mem_size = %d;"
		   "\nglobals.addr_space_size = %d; globals.act_concurrent_operations = %d;\n\n",
		   page_size, mem_size, addr_space_size, max_concurrent_operations);
    if (pthread_mutex_init(&globals.concurrent_operations_mutex, NULL) 
		== RETURN_ERROR)
		return RETURN_ERROR;
    if (pthread_cond_init(&globals.io_oveflow_cond, NULL) 
		== RETURN_ERROR)
		return RETURN_ERROR;
    if (pthread_mutex_init(&globals.logger_mutex, NULL) 
		== RETURN_ERROR)
		return RETURN_ERROR;
    if (pthread_mutex_init(&globals.select_mutex, NULL) 
		== RETURN_ERROR)
		return RETURN_ERROR;
	printf("mutexes ready!\n");
	init_strategy_metadata();
    printf("strategy init ok!\n");
	if (init_swap() == RETURN_ERROR)
        return RETURN_ERROR;
    if (init_pages() == RETURN_ERROR)
        return RETURN_ERROR;
    
	globals.active = true;
	return RETURN_SUCCESS;
}

int page_sim_get(unsigned a, uint8_t *v)
{
	printf("***********************BEGIN**********************\n");
	printf("PAGE_SIM_GET (a = %d, *v)\n", a);
	unsigned offset;
	page * current_page = get_page_wrapper(a, &offset);
    if (!current_page)
	{
		pthread_mutex_unlock(&current_page->meta->lock);
		return RETURN_ERROR;
	} 
	    /* mutex already gained! */
    *v = current_page->data[offset];
	printf("***********************END************************\n");
	return finish_operation(current_page);
}

int page_sim_set(unsigned a, uint8_t v)
{
	printf("***********************BEGIN**********************\n");
	printf("PAGE_SIM_SET (a = %d, v = %d)\n", a, v);
	unsigned offset;
    page * current_page = get_page_wrapper(a, &offset);
	current_page->meta->modified = true;
	if (!current_page) return RETURN_ERROR;
	/* mutex already gained! */
    current_page->data[offset] = v;
	printf("***********************END************************\n");
    return finish_operation(current_page);
}

int page_sim_end()
{
    globals.active = false;
    pthread_mutex_destroy(&globals.concurrent_operations_mutex);
    pthread_cond_destroy(&globals.io_oveflow_cond);
    pthread_mutex_destroy(&globals.logger_mutex);
    for (int i = 0; i < globals.mem_size; ++i)
        free(globals.pages[i].data);
    for (int i = 0; i < globals.addr_space_size; ++i)
        pthread_mutex_destroy(&globals.pages[i].meta->lock);
	return RETURN_SUCCESS;
}
/*****************************************************************************/