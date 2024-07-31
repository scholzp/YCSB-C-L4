#include <list>
#include <map>
#include <semaphore.h>
#include <pthread.h>
#include <cstdlib>
#include "lib/mock.h"
#include <stdio.h>
#include "lib/futex.h"

static std::map<pthread_descr, sem_t*> sema_map{};
static std::list<sem_t*> sem_p_l;
static int lock;
int out_lock;



void __pthread_acquire(int *sl) {
    while (!__sync_bool_compare_and_swap((sl), 0, 1)) while (*sl) __builtin_ia32_pause();
}

void __pthread_release(int * spinlock)
{
  __sync_synchronize();
  *spinlock = 0;
  __asm__ __volatile__ ("" : "=m" (*spinlock) : "m" (*spinlock));
}

pthread_descr thread_self() {
    return pthread_self();
}

int restart(pthread_descr thread) {
    // //printf("Restart %lu\n", thread);
    __pthread_acquire(&lock);
    int code = 0;
    if (auto entry = sema_map.find(thread); entry != sema_map.end()) {
        // //printf("W" );
        int val;
        sem_getvalue(entry->second, &val);
        if (val != 0) {
            //printf("Sema val before post = %d\n", val);
        }
        auto new_sema = entry->second;
        //printf("Waking things up; P=%p; Thread %p\n", new_sema, thread);
        if(0 != sem_post(new_sema)) {
            //printf("Wakeup failed, sema error!\n");
        }
        //printf("Wake succeeded!\n");
        if (val != 0) {
            //printf("Sema val after post = %d\n", val);
        }
    } else {
        //printf("Wakeup requested for thread %lu from %lu. NOT FOUND!!\n", thread, thread_self());
        code = -1;
    }
    __pthread_release(&lock);
    return code;
}

void suspend(pthread_descr thread) {
    __pthread_acquire(&lock);
    if (auto entry = sema_map.find(thread); entry != sema_map.end()) {
        //printf("Sleepy.... Thread: %p\n", thread);
        int val; 
        sem_getvalue(entry->second, &val);
        // if (val != 0)
        //     //printf("%lu: Sema val = %d\n",thread, val);
        __pthread_release(&lock);
        sem_wait(entry->second);
        sem_getvalue(entry->second, &val);
        if (val != 0) {
            //printf("%lu: Sema val after = %d\n",thread, val);
        }
    } else {
        auto new_sema = (sem_t *) malloc(sizeof(sem_t));
        sem_init(new_sema, 0, 0);
        sem_p_l.push_back(new_sema);
        sema_map.insert({thread, new_sema});
        int val; 
        sem_getvalue(new_sema, &val);
        //printf("Create sema for %p with value %d at address %p\n", thread, val, new_sema);
        __pthread_release(&lock);
        sem_wait(new_sema);
    }
}