/* alloc.c (dynamic memory allocation) */

/***********************************************************************
*  This code is part of GLPK (GNU Linear Programming Kit).
*
*  Copyright (C) 2000-2013 Andrew Makhorin, Department for Applied
*  Informatics, Moscow Aviation Institute, Moscow, Russia. All rights
*  reserved. E-mail: <mao@gnu.org>.
*
*  GLPK is free software: you can redistribute it and/or modify it
*  under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  GLPK is distributed in the hope that it will be useful, but WITHOUT
*  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
*  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
*  License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with GLPK. If not, see <http://www.gnu.org/licenses/>.
***********************************************************************/

#include <stdlib.h>
#include "glpenv.h"


#define ALIGN 16
/* some processors need data to be properly aligned, so this macro
 * defines the alignment boundary, in bytes, provided by glpk memory
 * allocation routines; looks like 16-byte alignment boundary is
 * sufficient for all 32- and 64-bit platforms (8-byte boundary is not
 * sufficient for some 64-bit platforms because of jmp_buf) */

#define MBD_SIZE (((sizeof(MBD) + (ALIGN - 1)) / ALIGN) * ALIGN)
/* size of memory block descriptor, in bytes, rounded up to multiple
 * of the alignment boundary */

/***********************************************************************
*  dma - dynamic memory allocation (basic routine)
*
*  This routine performs dynamic memory allocation. It is similar to
*  the standard realloc function, however, it provides every allocated
*  memory block with a descriptor, which is used for sanity checks on
*  reallocating/freeing previously allocated memory blocks as well as
*  for book-keeping the memory usage statistics. */

#ifdef HAVE_ENV

/**
 * Return the memory block descriptor for a memory block that was allocated by glp_alloc.
 */
static inline MBD* ptr_to_mbd(void* ptr)
{
    MBD* mbd = (MBD *)((char *)ptr - MBD_SIZE);
    xassert(mbd->self == mbd);
    return mbd;
}

/**
 * Take the mbd off the environment. Must only ever try to remove an mbd from the environment
 * it was allocated on
 */
static inline void _remove_from_env(MBD* mbd, ENV* env)
{
    xassert(mbd->env == env);

    MBD* before = mbd->prev;
    MBD* after = mbd->next;

    /* unlink ourselves */
    if(after) {
        after->prev = before;
    }
    if(before) {
        before->next = after;
    } else {
        env->mem_ptr = after;
    }

    /* update counters */
    if (GET_MEM_COUNT() < 1 || GET_MEM_TOTAL() < mbd->size) {
        xerror("unlinking mbd: memory deallocation error; inconsistent state\n");
    }
    ADD_MEM_COUNT(-1);
    ADD_MEM_TOTAL(-(mbd->size));
}

/**
 * Free an mbd from an env. If the mbd's env and the current env mismatch, the block will *not* be deallocated yet,
 * instead the deallocation will be deferred until the environment is freed
 */
static inline void _free_mbd(MBD* mbd, ENV* env) {
    if(mbd->env == env) {
        _remove_from_env(mbd, env);
        free(mbd);
    }
}

/**
 * Ensure that the requested allocation is reasonable
 */
static inline void _check_allocation(ENV* env, size_t size) 
{
    if (size > SIZE_T_MAX - MBD_SIZE) {
        xerror("block too large\n");
    }
    if (size + MBD_SIZE > GET_MEM_LIMIT() - GET_MEM_TOTAL()) {
        xerror("memory allocation limit exceeded\n");
    }
    if (GET_MEM_COUNT() == SIZE_T_MAX) {
        xerror("too many memory blocks allocated\n");
    }
}

/**
 * Finish the definition of an mbd, and prepend the memory-block-descriptor to the head of the linked-list of tracked
 * mbd's This happens in O(1)
 *
 * @param[in,out] mbd - The memory descriptor block we will prepend to the list
 * @param[in,out] env - where we will prepend the mbd
 * @param[in] size - What size we allocatd the mbd to.
 *
 */
static inline void _prepend_mbd_to_env(MBD* mbd, ENV* env, size_t size)
{
    xassert(mbd != NULL);
    xassert(env != NULL);
    mbd->self = mbd;
    mbd->size = size;
    mbd->env = env;
    mbd->prev = NULL;
    mbd->next = env->mem_ptr;

    if(env->mem_ptr != NULL) {
        env->mem_ptr->prev = mbd;
    }

    env->mem_ptr = mbd;
    ADD_MEM_COUNT(1);
    SET_MEM_CPEAK();

    ADD_MEM_TOTAL(size);
    SET_MEM_TPEAK();
}

/**
 * Calloc an mbd and add it to the env
 */
static inline void* _calloc_mbd(ENV* env, size_t size)
{
    MBD* mbd = NULL;
    if(size == 0) {
        return NULL;
    }
    _check_allocation(env, size);
    mbd = calloc(1, size + MBD_SIZE);
    xassert(mbd != NULL);
    _prepend_mbd_to_env(mbd, env, size);
    return mbd;
}

/**
 * Reallocate an mbd. This can *only* work if the mbd was previously allocated in this env. 
 * (if size is 0, will free the mbd instead)
 */
static inline void* _realloc_mbd(MBD* mbd, ENV* env, size_t size)
{
    if(size == 0) {
        _free_mbd(mbd, env);
        return NULL;
    }
    if(mbd->env != env) {
        xerror("ptr = %p; unable to reallocate from another environment\n", mbd);
    }
    _check_allocation(env, size);
    _remove_from_env(mbd, env);
    mbd = realloc(mbd, size + MBD_SIZE);
    xassert(mbd != NULL);
    _prepend_mbd_to_env(mbd, env, size);
    return mbd;
}

/***********************************************************************
*  NAME
*
*  glp_alloc - allocate memory block
*
*  SYNOPSIS
*
*  void *glp_alloc(int n, int size);
*
*  DESCRIPTION
*
*  The routine glp_alloc allocates a memory block of n * size bytes
*  long.
*
*  Note that being allocated the memory block contains arbitrary data
*  (not binary zeros!).
*
*  RETURNS
*
*  The routine glp_alloc returns a pointer to the block allocated.
*  To free this block the routine glp_free (not free!) must be used. */

void *glp_alloc(int n, int size)
{
    if (n < 1) {
        xerror("glp_alloc: n = %d; invalid parameter\n", n);
    }
    if (size < 1) {
        xerror("glp_alloc: size = %d; invalid parameter\n", size);
    }
    if ((size_t)n > SIZE_T_MAX / (size_t)size) {
        xerror("glp_alloc: n = %d, size = %d; block too large\n", n, size);
    }
    ENV* env = get_env_ptr();
    return (char*)_calloc_mbd(env, (size_t)n * (size_t)size) + MBD_SIZE;
}

/**********************************************************************/
/* reallocate memory block */
void *glp_realloc(void *ptr, int n, int size)
{     
    if (ptr == NULL) {
        xerror("glp_realloc: ptr = %p; invalid pointer\n", ptr);
    }
    if (n < 1) {
        xerror("glp_realloc: n = %d; invalid parameter\n", n);
    }
    if (size < 1) {
        xerror("glp_realloc: size = %d; invalid parameter\n", size);
    }
    if ((size_t)n > SIZE_T_MAX / (size_t)size) {
        xerror("glp_realloc: n = %d, size = %d; block too large\n", n, size);
    }
    ENV* env = get_env_ptr();
    return (char*)_realloc_mbd(ptr_to_mbd(ptr), env, (size_t)n * (size_t)size) + MBD_SIZE;
}

/***********************************************************************
*  NAME
*
*  glp_free - free (deallocate) memory block
*
*  SYNOPSIS
*
*  void glp_free(void *ptr);
*
*  DESCRIPTION
*
*  The routine glp_free frees (deallocates) a memory block pointed to
*  by ptr, which was previuosly allocated by the routine glp_alloc or
*  reallocated by the routine glp_realloc. */

void glp_free(void *ptr)
{
    if (ptr == NULL) {
        xerror("glp_free: ptr = %p; invalid pointer\n", ptr);
    }
    ENV* env = get_env_ptr();
    _free_mbd(ptr_to_mbd(ptr), env);
    return;
}

/***********************************************************************
*  NAME
*
*  glp_mem_limit - set memory usage limit
*
*  SYNOPSIS
*
*  void glp_mem_limit(int limit);
*
*  DESCRIPTION
*
*  The routine glp_mem_limit limits the amount of memory available for
*  dynamic allocation (in GLPK routines) to limit megabytes. */

void glp_mem_limit(int limit)
{     
      ENV *env = get_env_ptr();
      if (limit < 1) {
         xerror("glp_mem_limit: limit = %d; invalid parameter\n", limit);
      }
      SET_MEM_LIMIT(SIZE_T_MAX);
      return;
}

/***********************************************************************
*  NAME
*
*  glp_mem_usage - get memory usage information
*
*  SYNOPSIS
*
*  void glp_mem_usage(int *count, int *cpeak, size_t *total,
*     size_t *tpeak);
*
*  DESCRIPTION
*
*  The routine glp_mem_usage reports some information about utilization
*  of the memory by GLPK routines. Information is stored to locations
*  specified by corresponding parameters (see below). Any parameter can
*  be specified as NULL, in which case its value is not stored.
*
*  *count is the number of the memory blocks currently allocated by the
*  routines glp_malloc and glp_calloc (one call to glp_malloc or
*  glp_calloc results in allocating one memory block).
*
*  *cpeak is the peak value of *count reached since the initialization
*  of the GLPK library environment.
*
*  *total is the total amount, in bytes, of the memory blocks currently
*  allocated by the routines glp_malloc and glp_calloc.
*
*  *tpeak is the peak value of *total reached since the initialization
*  of the GLPK library envirionment. */

void glp_mem_usage(size_t *count, size_t *cpeak, size_t *total,
      size_t *tpeak)
{  
#ifdef HAVE_ENV
      ENV *env = get_env_ptr();
#endif
      if (count != NULL)
         *count = GET_MEM_COUNT();
      if (cpeak != NULL)
         *cpeak = GET_MEM_CPEAK();
      if (total != NULL)
         *total = GET_MEM_TOTAL();
      if (tpeak != NULL)
         *tpeak = GET_MEM_TPEAK();
      return;
}

#else

void *glp_alloc(int n, int size)
{
    return calloc((size_t)n, (size_t)size);
}

void *glp_realloc(void *ptr, int n, int size)
{
    return realloc(ptr, (size_t)n * (size_t)size);
}

void glp_free(void *ptr)
{
    free(ptr);
}

void glp_mem_limit(int limit)
{
    
}

void glp_mem_usage(size_t *count, size_t *cpeak, size_t *total,
                   size_t *tpeak)
{
    if (count != NULL)
        *count = 0;
    if (cpeak != NULL)
        *cpeak = 0;
    if (total != NULL)
        *total = 0;
    if (tpeak != NULL)
        *tpeak = 0;
    return;
}

#endif

/* eof */
