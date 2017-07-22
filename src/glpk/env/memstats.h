#pragma once
#ifndef GLPK_ENV_MEMSTATS_H
#define GLPK_ENV_MEMSTATS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef struct MEMSTATS {
    size_t problem_mem_count;
    size_t problem_mem_cpeak;
    size_t problem_mem_total;
    size_t problem_mem_tpeak;
} glp_memstats;

#ifdef HAVE_ENV
glp_memstats* glp_set_memstats(glp_memstats* new_stats);
// set the memstats on the current env
#endif

#ifdef __cplusplus
}
#endif 

#endif
