/******************************************************************************
 * pagesim.c																  *
 * Author: Krzysztof Wisniewski												  *
 *																			  *
 *****************************************************************************/

/**************************** included headers *******************************/
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <aio.h>

#include "page.h"
#include "strategy.h"
#include "pagesim.h"
#include "common.h"
/*****************************************************************************/

/**************************** global variables *******************************/
static struct
{
    page pages[513];
    uint8_t * frames[64];
    bool active;
	uint64_t global_counter; /* may overroll after 2^64 memory I/Os */ 
    unsigned page_size;
    unsigned mem_size;r
    unsigned addr_space_size;
    unsigned max_concurrent_operations;
	unsigned act_concurrent_operations;
	pthread_cond_t io_oveflow_cond;
	pthread_mutex_t concurrent_operations_mutex;
    int swap_fd;
    pagesim_callback logger;
} globals;
/*****************************************************************************/

/**************************** typedefs ***************************************/
typedef void (*page_access)(unsigned *, page *, unsigned *);
typedef void (*pagesim_callback)(int op, int arg1, int arg2);
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
static int init_pages()
{
	uint8_t frame[page_size];
	for (int i = 0; i < globals.page_size; ++i) frame[i] = 0;
	
	struct aiocb aio_init, * aio_p = &aio_init;
	aio_init.aio_fildes = globals.swap_fd;
	aio_init.aio_nbytes = globals.page_size;
	aio_init.aio_buf = &frame;
	aio_init.aio_sigevent.sigev_notify = SIGEV_NONE;
	
	for (int i = 0; i < globals.addr_space_size; ++i)
    {
		if (pthread_mutex_init(&pages[i].mutex, 0) == RETURN_ERROR) 
			return RETURN_ERROR;
	
		globals.pages[i].correct = false;
		globals.pages[i].page_index = i;
		globals.pages[i].reference_counter = 0;
		aio_init.aio_offset = i * globals.page_size;
		
		if (aio_write(&aio_init) == RETURN_ERROR) 
			return RETURN_ERROR;
		if (aio_suspend(&aio_p, NULL) == RETURN_ERROR) return RETURN_ERROR;
    }
	
	for (int i = 0; i < mem_size_; ++i)
    {
		globals.frames[i] = calloc(globals.page_size, sizeof(uint8_t));
		if (globals.frames[i] == NULL) return RETURN_ERROR;
        globals.pages[i].frame_number = i;
		globals.pages[i].correct = true;
    }
	return RETURN_SUCCESS;
}

static int init_swap() 
{
	if (globals.swap_fd = 
		open("swap.swp", O_CREAT | O_RDWR | 0666) == RETURN_ERROR)
		return RETURN_ERROR;
	return RETURN_SUCCESS;
}

static int translate_address(unsigned a, unsigned *page_num, unsigned *offset)
{
    *page_num = a / globals.page_size;
    *offset = a % globals.page_size;
	if (*offset > globals.page_size || *page_num > globals.addr_space_size)
		return RETURN_ERROR;
	else return RETURN_SUCCESS;
}

static int increase_io_operations_counter()
{
	if (pthread_mutex_lock(&globals.concurrent_operations_mutex) == -1)
		return -1;
	++globals.act_concurrent_operations;
	if (globals.act_concurrent_operations > globals.max_concurrent_operations) 
	{
		if (pthread_cond_wait(globals.io_oveflow_cond,
							  globals.concurrent_operations_mutex) == -1)
		{
			--globals.act_concurrent_operations;
			return -1;
		}
	}
	if (pthread_mutex_unlock(&globals.concurrent_operations_mutex) == -1)
	{
		--globals.act_concurrent_operations;
		return -1;
	}
	
}

static int decrease_io_operations_counter()
{
	if (pthread_mutex_lock(&globals.concurrent_operations_mutex) == -1)
	{	
		--globals.act_concurrent_operations;
		return NULL;
	}
	--globals.act_concurrent_operations;
	if (pthread_mutex_unlock(&globals.concurrent_operations_mutex) == -1)
	{
		--globals.act_concurrent_operations;
		return NULL;
	}
}

static page * get_page(unsigned page_num) 
{
	page * current_page;
	unsigned frame_num;
	if (!globals.pages[page_num].correct)
	{
		if (pthread_mutex_lock(&globals.pages[page_num].mutex) 
			== RETURN_ERROR)
			return NULL;
		current_page = select_page(globals.pages, globals.addr_space_size);
		current_page->reference_counter = 0;
		frame_num = current_page->frame_number;
		current_page->correct = false;
		
		struct aiocb aio_init;
		aio_init.aio_fildes = globals.swap_fd;
		aio_init.aio_offset = current_page->page_index * globals.page_size;
		aio_init.aio_nbytes = globals.page_size;
		aio_init.aio_buf = &globals.frames[frame_num];
		aio_init.aio_sigevent.sigev_notify = SIGEV_NONE;
		
		if (increase_io_operations_counter() == -1) return NULL;
		if (current_page->modified) {
			aio_write(&aio_init);
			aio_suspend(&aio_init, NULL);
		}
		aio_init.aio_offset = page_num * globals.page_size;
		aio_read(&aio_init);
		aio_suspend(&aio_init, NULL);
		if (decrease_io_operations_counter() == -1) return NULL;
		
		current_page = &globals.pages[page_num];
		current_page->frame_number = frame_num;
	}
	else current_page = &globals.pages[page_num];
	return current_page;
}

static void set_value(unsigned *v, page * current_page, unsigned * page_offset)
{
	globals.frames[current_page->frame_number][*page_offset] = *v;
}

static void get_value(unsigned *v, page * current_page, unsigned * page_offset)
{
	*v = globals.frames[current_page->frame_number][*page_offset];
}

static int page_sim_access_page(page_access perform_maintanance,
								unsigned * v,
								unsigned * a)
{
	unsigned page_num, offset;
	if (!globals.active)
	{
		errno = ENOPROTOOPT;
		return RETURN_ERROR;
	}
	if (translate_address(*a, page_num, offset) == RETURN_ERROR)
	{
		errno = EFAULT;
		return RETURN_ERROR;
	}
	
	page * current_page = get_page(page_num);
	if (current_page == NULL) return RETURN_ERROR;
	current_page->reference_counter++;
	perform_maintanance(v, current_page, offset);
	
	if (pthread_mutex_unlock(&current_page->mutex) == RETURN_ERROR)
		return RETURN_ERROR;
}
/*****************************************************************************/

/**************************** core implementation ****************************/
int page_sim_init(unsigned page_size, unsigned mem_size,
        unsigned addr_space_size, unsigned max_concurrent_operations,
        pagesim_callback callback)
{
    globals.page_size = page_size;
    globals.mem_size = mem_size;
    globals.addr_space_size = addr_space_size;
	globals.global_counter = 0;
	globals.act_concurrent_operations = 0;
	globals.concurrent_operations_mutex = PTHREAD_MUTEX_INITIALIZER;
	globals.io_oveflow_cond = PTHREAD_CONT_INITIALIZER;
	globals.logger = callback;
	if (init_swap() == RETURN_ERROR) return RETURN_ERROR;
    if (init_pages() == RETURN_ERROR) return RETURN_ERROR;
	globals.active = true;
}

int page_sim_get(unsigned a, uint8_t *v)
{
	return page_sim_access_page((page_access) get_value, v, &a);
} 

int page_sim_set(unsigned a, uint8_t v)
{
	return page_sim_access_page((page_access) set_value, &v, &a)
}

int page_sim_end()
{
    globals.active = false;
    
    for (i = 0; i < globals.mem_size; ++i)
    {
        free(frame[i]);
    }
}
/*****************************************************************************/