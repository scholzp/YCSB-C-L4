#include <cstdint>
#include <mutex>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <vector>
#include <time.h>
#include <set>
#include "futex.h"
#include <stdio.h>

class wrapped_mutex: std::mutex {
    public:
        void lock() {
            // printf("lock\n");
            uint64_t c;
            c = __sync_val_compare_and_swap(&__status, 0, 1); 
        	if (c != 0) {
                // printf(".\n");
        		do {
        			if (c == 2 || __sync_val_compare_and_swap(&__status, 1, 2) != 0) {
        				futex_wait(&__status, 2, NULL, 0);
        			}
        		} while ((c = __sync_val_compare_and_swap(&__status, 0, 2)) != 0);
        	}
        }

        bool try_lock() {
            printf("trylock\n");
            return std::mutex::try_lock();
        }

        void unlock() {
           	if (__sync_sub_and_fetch(&__status, 1) != 0) {
                // We *really* don't want to miss any wait queue entries...
		        __status = 0;
                __sync_synchronize();
		        futex_wake(&__status, 1, 0);
        	}
        }

        wrapped_mutex()
            {
                __status = 0;
            };
        ~wrapped_mutex() {
            printf("Detroyed!!\n");
        };
    
    private:
        uint64_t __status;
        int locks_lock;

};