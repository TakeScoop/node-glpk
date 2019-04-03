/* env.c (GLPK environment initialization/termination) */

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

#include "stdc.h"
#include "glpenv.h"
#include "../glpk.h"

#ifdef HAVE_ENV
int _glp_init_env(ENV *env)
{     
      memset(env, 0, sizeof(*env));
      snprintf(env->version, sizeof(*env->version) - 1, "%d.%d", GLP_MAJOR_VERSION, GLP_MINOR_VERSION);

      env->self = env;
      env->term_buf = malloc(TBUF_SIZE);
      if (env->term_buf == NULL) return 2;
      env->term_out = GLP_ON;
      env->err_buf = malloc(EBUF_SIZE);
      if (env->err_buf == NULL) return 2;
      env->mem_limit = SIZE_T_MAX;
      return 0;
}
/***********************************************************************
*  NAME
*
*  glp_init_env - initialize GLPK environment
*
*  SYNOPSIS
*
*  int glp_init_env(void);
*
*  DESCRIPTION
*
*  The routine glp_init_env initializes the GLPK environment. Normally
*  the application program does not need to call this routine, because
*  it is called automatically on the first call to any API routine.
*
*  RETURNS
*
*  The routine glp_init_env returns one of the following codes:
*
*  0 - initialization successful;
*  1 - environment has been already initialized;
*  2 - initialization failed (insufficient memory);
*  3 - initialization failed (unsupported programming model). */
int glp_init_env(void)
{
      ENV *env = NULL;
      int ok;
      /* check if the programming model is supported */
      ok = (CHAR_BIT == 8 && sizeof(char) == 1 &&
         sizeof(short) == 2 && sizeof(int) == 4 &&
         (sizeof(void *) == 4 || sizeof(void *) == 8));
      if (!ok) {
         return 3;
      }
      /* check if the environment is already initialized */
      if (tls_get_ptr() != NULL) {
          return 1;
      }

      /* allocate and initialize the environment block */
      env = calloc(1,sizeof(*env));
      if (!env || _glp_init_env(env) != 0) {
          free(env->err_buf);
          free(env->term_buf);
          free(env);
          return 2;
      }
      tls_set_ptr(env);
      return 0;
}

#endif

/***********************************************************************
*  NAME
*
*  get_env_ptr - retrieve pointer to environment block
*
*  SYNOPSIS
*
*  #include "glpenv.h"
*  ENV *get_env_ptr(void);
*
*  DESCRIPTION
*
*  The routine get_env_ptr retrieves and returns a pointer to the GLPK
*  environment block.
*
*  If the GLPK environment has not been initialized yet, the routine
*  performs initialization. If initialization fails, the routine prints
*  an error message to stderr and terminates the program.
*
*  RETURNS
*
*  The routine returns a pointer to the environment block. */
#ifdef HAVE_ENV
ENV *get_env_ptr(void)
{     ENV *env = tls_get_ptr();
      /* check if the environment has been initialized */
      if (env == NULL)
      {  /* not initialized yet; perform initialization */
         if (glp_init_env() != 0)
         {  /* initialization failed; display an error message */
            fprintf(stderr, "GLPK initialization failed\n");
            fflush(stderr);
            /* and abnormally terminate the program */
            abort();
         }
         /* initialization successful; retrieve the pointer */
         env = tls_get_ptr();
      }
      /* check if the environment block is valid */
      if (env->self != env)
      {  fprintf(stderr, "Invalid GLPK environment\n");
         fflush(stderr);
         abort();
      }

      return env;
}
#endif

#ifdef HAVE_ENV
/***********************************************************************
*  NAME
*
*  glp_version - determine library version
*
*  SYNOPSIS
*
*  const char *glp_version(void);
*
*  RETURNS
*
*  The routine glp_version returns a pointer to a null-terminated
*  character string, which specifies the version of the GLPK library in
*  the form "X.Y", where X is the major version number, and Y is the
*  minor version number, for example, "4.16". */
const char *glp_version(void)
{     
    ENV *env = get_env_ptr();
    return env->version;
}
#endif

#ifdef HAVE_ENV
int _glp_free_env(ENV *env) {

    /* check if the environment is active */
    if (env == NULL) {
        return 1;
    }

    /* check if the environment block is valid */
    if (env->self != env) {
        fprintf(stderr, "Invalid GLPK environment\n");
        fflush(stderr);
        abort();
    }

    /* close handles to shared libraries */
    if (env->h_odbc != NULL) {
        xdlclose(env->h_odbc);
    }
    if (env->h_mysql != NULL) {
        xdlclose(env->h_mysql);
    }
    /* close text file used for copying terminal output */
    if (env->tee_file != NULL) {
        fclose(env->tee_file);
    }

    MBD* p = env->mem_ptr;
    MBD* q = (p) ? p->next : NULL;

    // quick check for loops;
    while(p) {
        xassert(p != q);
        p = p->next;
        q = (q && q->next) ? q->next->next : NULL;
    }
    /* invalidate the environment block */
    while (env->mem_ptr != NULL) {
        p = env->mem_ptr;
        env->mem_ptr = p->next;
        free(p);
    }
    env->self = NULL;
    /* free memory allocated to the environment block */
    free(env->term_buf);
    free(env->err_buf);
    free(env);
    return 0;
}
/***********************************************************************
*  NAME
*
*  glp_free_env - free GLPK environment
*
*  SYNOPSIS
*
*  int glp_free_env(void);
*
*  DESCRIPTION
*
*  The routine glp_free_env frees all resources used by GLPK routines
*  (memory blocks, etc.) which are currently still in use.
*
*  Normally the application program does not need to call this routine,
*  because GLPK routines always free all unused resources. However, if
*  the application program even has deleted all problem objects, there
*  will be several memory blocks still allocated for the library needs.
*  For some reasons the application program may want GLPK to free this
*  memory, in which case it should call glp_free_env.
*
*  Note that a call to glp_free_env invalidates all problem objects as
*  if no GLPK routine were called.
*
*  RETURNS
*
*  0 - termination successful;
*  1 - environment is inactive (was not initialized). */
int glp_free_env(void)
{
      ENV *env = (ENV*) tls_get_ptr();
      int r = _glp_free_env(env);
      if (r == 0) {
          tls_set_ptr(NULL);
      }
      return r;
}
#endif

#ifdef HAVE_ENV
int _glp_free_env(ENV *env);
int _glp_init_env(ENV *env);

/**
 * Get env readlock
 */
static inline int environ_state_rdlock(glp_environ_state_t *env_state)
{
    return pthread_rwlock_rdlock(&env_state->env_lock);
}

/**
 * Get env lock
 */
static inline int environ_state_wrlock(glp_environ_state_t *env_state)
{
    return pthread_rwlock_wrlock(&env_state->env_lock);
}
/**
 * Free lock
 */
static inline int environ_state_unlock(glp_environ_state_t* env_state)
{
    return pthread_rwlock_unlock(&env_state->env_lock);
}

/**
 * Reentrant and threadsafe function for migrating all environment data from the thread-local environment to the given env_state 
 * Call this whenever a thread terminates to ensure all thread-local state is preserved.
 */
void glp_env_tls_finalize_r(glp_environ_state_t* env_state)
{
    ENV *env = tls_get_ptr();
    if(env == NULL) return;

    /* Outside of the critical section, find the end of this thread's
     * linked list; because last_node was set from tail_ptr, last_node->next
     * *should* be NULL, but just in case */
    MBD* last_node = NULL;
    MBD* first_node = env->mem_ptr;
    MBD* p = first_node;
    MBD* q = (p) ? p->next : NULL;

    // We are removing the env that these were associated with, so we must associate them with the
    // env_state's env
    while(p) {
        xassert(p != q);
        p->env = env_state->env;
        last_node = p;
        p = p->next;
        q = (q && q->next) ? q->next->next : NULL;
    }

    /* In the critical section, we'll just prepend this threads linked list to
     * the existing linked-list */
    xassert(environ_state_wrlock(env_state) == 0);
    {
        if(last_node) { 
            if(env_state->env->mem_ptr != NULL) env_state->env->mem_ptr->prev = last_node;
            last_node->next = env_state->env->mem_ptr;
            env_state->env->mem_ptr = first_node;
        }

        if(env->mem_cpeak_tls + env_state->env->mem_count > env_state->env->mem_cpeak) {
            env_state->env->mem_cpeak = env->mem_cpeak_tls + env_state->env->mem_count;
        }
        if(env->mem_tpeak_tls + env_state->env->mem_total > env_state->env->mem_tpeak) {
            env_state->env->mem_tpeak = env->mem_tpeak_tls + env_state->env->mem_total;
        }

        env_state->env->mem_total += env->mem_total_tls;
        env_state->env->mem_count += env->mem_count_tls;
    }
    xassert(environ_state_unlock(env_state) == 0);
    env->mem_ptr = NULL;
    glp_free_env();
}

/**
 * This initializes thread-local storage from a given env_state and info.
 * @param[in] env_state - environment to intialize TLS from
 * @param[in] info - info to initialize env->term_info with if specified
 *                   (supersedes any info in the env_state).
 */
void glp_env_tls_init_r(glp_environ_state_t *env_state, void* info) 
{
    ENV *env = get_env_ptr();
    if(env->env_tls_init_flag) {
        return;
    }
    env->env_tls_init_flag = 1;

    if (info != NULL) env->term_info = info;

    xassert(environ_state_rdlock(env_state) == 0);
    {
        env->mem_count = env_state->env->mem_count;
        env->mem_total = env_state->env->mem_total;
        env->mem_cpeak = env_state->env->mem_cpeak;
        env->mem_tpeak = env_state->env->mem_tpeak;
        env->mem_limit = env_state->env->mem_limit;

        env->tee_file = env_state->env->tee_file;
        env->err_hook = env_state->env->err_hook;
        env->term_hook = env_state->env->term_hook;
        xassert(env_state->env->term_hook);
        if (env->term_info == NULL) env->term_info = env_state->env->term_info;
    }
    xassert(environ_state_unlock(env_state) == 0);
}

/**
 * Initialize environment state (per problem, etc)
 * @param[in] default_info - The resulting env_state's term_info will be set to default_info
 * @param[in] nodeHookCallback - The callback to set term_hook to.
 *
 * @return a heap allocated and initialized env_state
 */
glp_environ_state_t* glp_init_env_state(void* default_info, int (*nodeHookCallback)(void*, const char*))
{
    glp_environ_state_t* env_state = calloc(1, sizeof(*env_state));
    pthread_rwlock_init(&env_state->env_lock, NULL);
    env_state->env = calloc(1, sizeof(*env_state->env));
    _glp_init_env(env_state->env);
    env_state->env->term_info = default_info;
    env_state->env->term_hook = nodeHookCallback;
    return env_state;
}

/**
 * Free all resources associated with ane env
 */
void glp_free_env_state(glp_environ_state_t *env_state)
{
    glp_env_tls_finalize_r(env_state);
    pthread_rwlock_destroy(&env_state->env_lock);
    _glp_free_env(env_state->env);
    free(env_state);
}

struct glp_memory_counters glp_counters_from_state(glp_environ_state_t* env_state)
{
    struct glp_memory_counters counters;
    xassert(environ_state_rdlock(env_state) == 0);
    {
        counters.mem_count = env_state->env->mem_count;
        counters.mem_total = env_state->env->mem_total;
        counters.mem_cpeak = env_state->env->mem_cpeak;
        counters.mem_tpeak = env_state->env->mem_tpeak;
    }
    xassert(environ_state_unlock(env_state) == 0);
    return counters;
}
#endif
