#ifndef _SECMALLOC_H
#define _SECMALLOC_H

#include <stdlib.h>

#ifdef MSM_ALIASES
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

#define MSM_MY static
#else
#define MSM_MY
#endif

MSM_MY void *my_malloc(size_t size);
MSM_MY void my_free(void *ptr);
MSM_MY void *my_calloc(size_t nmemb, size_t size);
MSM_MY void *my_realloc(void *ptr, size_t size);

#endif
