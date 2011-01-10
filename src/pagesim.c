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
    unsigned page_size;
    unsigned mem_size;
    unsigned addr_space_size;
    unsigned max_concurrent_operations;
	unsigned act_concurrent_operations;
    int swap_fd;
    pagesim_callback logger;
	pthread_cond_t io_oveflow_cond;
	pthread_mutex_t concurrent_operations_mutex;

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
		if ((err = pthread_mutex_init(&pages[i].mutex, 0)) == -1) return -1;
		globals.pages[i].correct = false;
		globals.pages[i].page_index = i;
		aio_init.aio_offset = i * globals.page_size;
		if (aio_write(&aio_init) == -1) return -1;
		if (aio_suspend(&aio_p, NULL) == -1) return -1;
    }
	for (int i = 0; i < mem_size_; ++i)
    {
		globals.frames[i] = calloc(globals.page_size, sizeof(uint8_t));
		if (globals.frames[i] == NULL) return -1;
        globals.pages[i].frame_number = i;
		globals.pages[i].correct = true;
    }
	return 0;
}

static int init_swap() 
{
	if (globals.swap_fd = open("swap.swp", O_CREAT | O_RDWR | 0666) == -1)
		return -1;
	return 0;
}

static int translate_address(unsigned a, unsigned *page_num, unsigned *offset)
{
    *page_num = a / globals.page_size;
    *offset = a % globals.page_size;
	if (*offset > globals.page_size || *page_num > globals.addr_space_size)
		return -1;
	else return 0;
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
		if (pthread_mutex_lock(&globals.pages[page_num].mutex) == -1)
			return NULL;
		current_page = select_page(globals.pages);
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
/*****************************************************************************/

/**************************** core implementation ****************************/
int page_sim_init(unsigned page_size, unsigned mem_size,
        unsigned addr_space_size, unsigned max_concurrent_operations,
        pagesim_callback callback)
{
    globals.page_size = page_size;
    globals.mem_size = mem_size;
    globals.addr_space_size = addr_space_size;
    globals.max_concurrent_operations = max_concurrent_operations;
	globals.concurrent_operations_mutex = PTHREAD_MUTEX_INITIALIZER;
	globals.io_oveflow_cond = PTHREAD_COND_INITIALIZER;
	globals.act_concurrent_operations = 0;
    globals.logger = callback;
	if (init_swap() == -1) return -1;
    if (init_pages() == -1) return -1;
	globals.active = true;
}

int page_sim_get(unsigned a, uint8_t *v)
{
    unsigned page_num, offset;
    if (!globals.active) 
	{
		errno = ;
		return -1;
	}
	if (translate_address(a, page_num, offset) == -1)
	{
		errno = EFAULT;
		return -1;
	}
	page * current_page = get_page(page_num);
	if (current_page == NULL) return -1;
	uint8_t * current_frame = globals.frames[current_page->frame_number];
	
	if (pthread_mutex_unlock(&current_page->mutex) == -1) return -1;
	return 0;	
}

int page_sim_set(unsigned a, uint8_t v)
{
    if (!globals.active)
	{
		errno = ENOPROTOOPT;
		return -1;
	}
	if (translate_address(a, page_num, offset) == -1)
	{
		errno = EFAULT;
		return -1;
	}
	
}

int page_sim_end()
{
    globals.active = false;
    
    for (i = 0; i < globals.mem_size; ++i)
    {
        free( frame[i]);
    }
}
/*****************************************************************************/