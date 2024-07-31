#include <cstdint>
#include <list>
#include <map>
#include <semaphore.h>
#include <thread>
#include <pthread.h>
#include <cstdlib>
#include <stdio.h>

typedef pthread_t pthread_descr;

void __pthread_acquire(int *sl);
void __pthread_release(int * spinlock);

extern int out_lock;
pthread_descr thread_self();

int restart(pthread_descr thread);

void suspend(pthread_descr thread) ;


#define DEBUG_MSG(msg)              \
    __pthread_acquire(&out_lock); \
    printf("TSC %llu: Thread %lu  Line: %lu: %s\n",__builtin_ia32_rdtsc(), thread_self(), __LINE__, msg); \
    __pthread_release(&out_lock); \
