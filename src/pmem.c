#include "pmem.h"
#include "pstate.h"


/* minimum memory block size of an array */
#define PAVC_MINARRSIZE	8


void *pavc_mem_growarray_(pavc_State *pavc, void *block, unsigned int *sizep, 
				unsigned int len, unsigned int limit, int elemsize)
{
	unsigned int size;

	size = *sizep;
	if (p_likely(len + 1 <= size))
		return block;
	if (p_unlikely(size >= (limit >> 1))) {
		if (p_unlikely(size >= limit))
			pavc_state_error(pavc, "array size limit reached");
		size = limit;
		pavc_assert(size >= PAVC_MINARRSIZE);
	} else {
		size <<= 1;
		if (size < PAVC_MINARRSIZE)
			size = PAVC_MINARRSIZE;
	}
	block = pavc_mem_realloc(pavc, block, *sizep * elemsize, size * elemsize);
	*sizep = size;
	return block;
}


void *pavc_mem_realloc(pavc_State *pavc, void* ptr, size_t osize, size_t size)
{
	ptr = pavc->alloc(ptr, pavc->ud, osize, size);
	if (p_unlikely(ptr == NULL))
		pavc_state_error(pavc, "out of memory (realloc)");
	return ptr;
}


void *pavc_mem_malloc(pavc_State *pavc, size_t size)
{
	void *ptr;

	ptr = pavc->alloc(NULL, pavc->ud, 0, size);
	if (p_unlikely(ptr == NULL))
		pavc_state_error(pavc, "out of memory (malloc)");
	return ptr;
}


void pavc_mem_free(pavc_State *pavc, void *block, size_t osize)
{
	pavc->alloc(block, pavc->ud, osize, 0);
}
