#ifndef PAVCCOMMON_H
#define PAVCCOMMON_H


#include <pulse/pulseaudio.h>
#include <stddef.h>


#if defined(PAVC_ASSERT)
#undef NDBG
#include <assert.h>
#define pavc_assert(e)		assert(e)
#else
#define pavc_assert(e)		((void)(0))
#endif


/* compiler branch prediction macros */
#if defined(__GNUC__)
#define p_likely(cond)		__builtin_expect((cond) != 0, 1)
#define p_unlikely(cond)	__builtin_expect((cond) != 0, 0)
#else
#define p_likely(cond)		cond
#define p_unlikely(cond)	cond
#endif


/* function does not return compiler attribute */
#if defined(__GNUC__)
#define p_noret		void __attribute__((noreturn))
#elif defined(_MSC_VER) && _MSC_VER >= 1200
#define p_noret		void __declspec(noreturn)
#else
#define p_noret		void
#endif


#define UNUSED(x) (void)(x)


/* state */
typedef struct pavc_State pavc_State;


/* allocator */
typedef void *(*pavc_Allocfunction)(void *block, void *ud, size_t osize, size_t nsize);


#endif
