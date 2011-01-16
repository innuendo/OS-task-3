#ifndef _COMMON_H
#define _COMMON_H

#define RETURN_ERROR -1
#define RETURN_SUCCESS 0

#define MIN_PAGE_SIZE 4
#define MAX_PAGE_SIZE 512
#define MIN_MEM_SIZE 1
#define MAX_MEM_SIZE 64
#define MIN_ADDR_SPACE_SIZE 1
#define MAX_ADDR_SPACE_SIZE 512
#define MIN_MAX_CONCURRENT_OPERATIONS 1
#define MAX_MAX_CONCURRENT_OPERATIONS 64 

#define BETWEEN(x,y,z) \
	(x) <= (z) && (z) <= (y)

#endif