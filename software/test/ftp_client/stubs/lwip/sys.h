#include <stdint.h>
#include <time.h>
static inline uint32_t sys_now() { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (uint32_t)(t.tv_sec*1000+t.tv_nsec/1000000); }
