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

#include "page.h"
#include "strategy.h"
#include "pagesim.h"
#include "common.h"
/*****************************************************************************/

/**************************** global variables *******************************/
static struct
{
    page pages[512];
    uint8_t * frames[64];
    bool active;
    unsigned page_size;
    unsigned mem_size;
    unsigned addr_space_size;
    unsigned max_concurrent_operations;
    unsigned act_concurrent_operations;
    pthread_cond_t io_oveflow_cond;
    pthread_mutex_t concurrent_operations_mutex;
    pthread_mutex_t logger_mutex;
    int swap_fd;
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

static int init_pages()
{
    uint8_t frame[globals.page_size];
    for (int i = 0; i < globals.page_size; ++i)
        frame[i] = 0;
    
    struct aiocb * aio_init_table[1];
	struct aiocb aio_init;
	aio_init_table[0] = &aio_init;
    aio_init.aio_fildes = globals.swap_fd;
    aio_init.aio_nbytes = globals.page_size;
    aio_init.aio_buf = &frame;
    aio_init.aio_sigevent.sigev_notify = SIGEV_NONE;
    
    for (int i = 0; i < globals.addr_space_size; ++i)
    {
        if (pthread_mutex_init(&globals.pages[i].lock, 0) == RETURN_ERROR)
            return RETURN_ERROR;
        
        globals.pages[i].correct = false;
        globals.pages[i].page_index = i;
        globals.pages[i].reference_counter = 0;
        globals.pages[i].frame_number = -1;
        globals.pages[i].modified = false;
        aio_init.aio_offset = i * globals.page_size;
        
        if (aio_write(&aio_init) == RETURN_ERROR)
            return RETURN_ERROR;
        if (aio_suspend((const struct aiocb * const *)aio_init_table, 1, NULL)
			== RETURN_ERROR)
            return RETURN_ERROR;
    }
    
    for (int i = 0; i < globals.mem_size; ++i)
    {
        globals.frames[i] = calloc(globals.page_size, sizeof(uint8_t));
        if (globals.frames[i] == NULL)
            return RETURN_ERROR;
        globals.pages[i].frame_number = i;
        globals.pages[i].correct = true;
    }
    return RETURN_SUCCESS;
}

static int init_swap()
{
    if ((globals.swap_fd = open("swap.swp", O_CREAT | O_RDWR | 0666))
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
    ++globals.act_concurrent_operations;
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

static void aiocb_setup(struct aiocb *aio_init, unsigned page_index,
                        unsigned frame_num)
{
    aio_init->aio_fildes = globals.swap_fd;
    aio_init->aio_offset = page_index * globals.page_size;
    aio_init->aio_nbytes = globals.page_size;
    aio_init->aio_buf = &globals.frames[frame_num];
    aio_init->aio_sigevent.sigev_notify = SIGEV_NONE;
}

static page * get_page(unsigned page_num)
/*
 *  op = 0 -> read, op = 1 -> write
 */
{
    page * current_page;
    unsigned frame_num;
    
    if (pthread_mutex_lock(&globals.pages[page_num].lock) != RETURN_SUCCESS)
        return NULL;
    
    if (!globals.pages[page_num].correct)
    {
        
        struct aiocb * aio_init_table[1];
		struct aiocb aio_init;
		aio_init_table[0] = &aio_init;
        /* saving selected page to disk */
        current_page = select_page(globals.pages, globals.addr_space_size);
        if (current_page == NULL)
            return NULL;
        /* mutex for page to be replaced already gained */
        
        current_page->reference_counter = 0;
        frame_num = current_page->frame_number;
        current_page->correct = false;
        current_page->modified = false;
        aiocb_setup(&aio_init, current_page->page_index, frame_num);
        
        if (increase_io_operations_counter() == RETURN_ERROR)
            return NULL;
        if (current_page->modified)
        {
            callback_wrapper(2, current_page->page_index, frame_num);
            if (aio_write(&aio_init) == RETURN_ERROR)
				return NULL;
            if (aio_suspend((const struct aiocb * const *)aio_init_table, 1, NULL)
				== RETURN_ERROR)
				return NULL;
            callback_wrapper(3, current_page->page_index, frame_num);
        }
        if (pthread_mutex_unlock(&current_page->lock) != RETURN_SUCCESS)
            return NULL;
        aiocb_setup(&aio_init, page_num, frame_num);
        callback_wrapper(4, page_num, frame_num);
        aio_read(&aio_init);
        aio_suspend((const struct aiocb * const *)aio_init_table, 1, NULL);
        callback_wrapper(5, page_num, frame_num);
        if (decrease_io_operations_counter() == RETURN_ERROR)
            return NULL;
        
        current_page = &globals.pages[page_num];
        current_page->frame_number = frame_num;
    }
    else
        current_page = &globals.pages[page_num];
    current_page->reference_counter++;
    update_metadata(page_num);
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
    globals.act_concurrent_operations = 0;
    pthread_mutex_init(&globals.concurrent_operations_mutex, NULL);
    pthread_cond_init(&globals.io_oveflow_cond, NULL);
    globals.logger = callback;
    pthread_mutex_init(&globals.logger_mutex, NULL);
    init_metadata();
    if (init_swap() == RETURN_ERROR)
        return RETURN_ERROR;
    if (init_pages() == RETURN_ERROR)
        return RETURN_ERROR;
    globals.active = true;
	return RETURN_SUCCESS;
}

int page_sim_get(unsigned a, uint8_t *v)
{
    unsigned page_num, offset;
    if (!globals.active)
    {
        errno = ENOPROTOOPT;
        return RETURN_ERROR;
    }
    if (translate_address(a, &page_num, &offset) == RETURN_ERROR)
    {
        errno = EFAULT;
        return RETURN_ERROR;
    }
    globals.logger(1, page_num, 0);
    page * current_page = get_page(page_num);
    /* mutex already gained! */
    if (current_page == NULL)
        return RETURN_ERROR;
    uint8_t * current_frame = globals.frames[current_page->frame_number];
    *v = current_frame[offset];
    if (pthread_mutex_unlock(&current_page->lock) == RETURN_ERROR)
        return RETURN_ERROR;
    
    return RETURN_SUCCESS;
}

int page_sim_set(unsigned a, uint8_t v)
{
    unsigned page_num, offset;
    if (!globals.active)
    {
        errno = ENOPROTOOPT;
        return RETURN_ERROR;
    }
    if (translate_address(a, &page_num, &offset) == RETURN_ERROR)
    {
        errno = EFAULT;
        return RETURN_ERROR;
    }
    callback_wrapper(1, page_num, 0);
    page * current_page = get_page(page_num);
    /* mutex already gained! */
    if (current_page == NULL)
        return RETURN_ERROR;
    uint8_t * current_frame = globals.frames[current_page->frame_number];
    current_frame[offset] = v;
    if (pthread_mutex_unlock(&current_page->lock) == RETURN_ERROR)
        return RETURN_ERROR;
    callback_wrapper(6, current_page->page_index, current_page->frame_number);
    return RETURN_SUCCESS;
}

int page_sim_end()
{
    globals.active = false;
    pthread_mutex_destroy(&globals.concurrent_operations_mutex);
    pthread_cond_destroy(&globals.io_oveflow_cond);
    pthread_mutex_destroy(&globals.logger_mutex);
    for (int i = 0; i < globals.mem_size; ++i)
        free(globals.frames[i]);
    for (int i = 0; i < globals.addr_space_size; ++i)
        pthread_mutex_destroy(&globals.pages[i].lock);
	return RETURN_SUCCESS;
}
/*****************************************************************************/