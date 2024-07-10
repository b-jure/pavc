#ifndef PAVCMEM_H
#define PAVCMEM_H

#include "pcommon.h"

#define pavc_mem_freearray(p,b,size)	pavc_mem_free(p,b,(size)*sizeof(*(b)))

#define pavc_mem_growarray(p,b,szp,len,l,t) \
	((b) = (t*)pavc_mem_growarray_(p, b, szp, len, l, sizeof(t)))

void *pavc_mem_realloc(pavc_State *pavc, void* ptr, size_t osize, size_t size);
void *pavc_mem_malloc(pavc_State *pavc, size_t size);
void pavc_mem_free(pavc_State *pavc, void *ptr, size_t osize);

void *pavc_mem_growarray_(pavc_State *pavc, void *block, unsigned int *sizep, 
				unsigned int len, unsigned int limit, int elemsize);

#endif
