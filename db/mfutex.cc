#include "lib/futex.h"
// #include "internals.h"
// #include "spinlock.h"
// #include "restart.h"
#include "errno.h"
#include "lib/mock.h"
#include <cstdio>
#include <cstdlib>
#include <stdio.h>

int global_futex_sp = 0;

struct futex futex_list[MAX_FUTEX_COUNT];
unsigned long futex_last_used_index;
unsigned long futex_first_free_index;

void inline __sl_acquire(int *sl) {
    while (!__sync_bool_compare_and_swap((sl), 0, 1)) while (*sl) __builtin_ia32_pause();
}

void inline  __sl_release(int * spinlock)
{
  __sync_synchronize();
  *spinlock = 0;
  __asm__ __volatile__ ("" : "=m" (*spinlock) : "m" (*spinlock));
}

struct wait_node* register_waiter(struct futex* f, pthread_descr descr){
	for (size_t index = 0; index < MAX_THREADS_PER_FUTEX; ++index) {
		if (0 == f->wait_queue[index].slot_is_used) {
			//printf("Thread %p; tsc(): %llu: Register waiter at %lu\n", thread_self(), __builtin_ia32_rdtsc(), index);
			f->wait_queue[index].slot_is_used = 1;
			f->wait_queue[index].descr = descr;
			return &f->wait_queue[index];
		}
	}
	// No free slots, we return an error
	//printf("Thread %p; tsc(): %llu: Return without registration\n",thread_self(), __builtin_ia32_rdtsc());
	return NULL;
}

int register_futex(uint64_t *uaddress, struct futex **out) {
	int first_free_slot = -1;
	for (size_t index = 0; index < MAX_FUTEX_COUNT; ++index) {
		// Check that no futex for given uaddress exists
		if (futex_list[index].uaddress == uaddress) {
			// //printf("Found futex for address %p; Index = %lu\n", uaddress, index);
			*out = &futex_list[index];
			return 0;
		}
		// Find first free slot
		if ((-1 == first_free_slot) && (0 == futex_list[index].slot_is_used)) {
			first_free_slot = index;
			break;
		}
	}
	// //printf("Create futex for address %p\n", uaddress);
	if (first_free_slot != -1) {
		// We are here because we found a free slot and the address is not yet 
		// associated to a futex. We allocate the slot.
		futex_list[first_free_slot].slot_is_used = 1;
		futex_list[first_free_slot].uaddress = uaddress;
		*out = &futex_list[first_free_slot];
		return 1;
	}
	// No free slots, we return an error
	*out = NULL;
	return -1;
}

int find_futex(uint64_t *uaddress, struct futex **out){
	for (size_t index = 0; index < MAX_FUTEX_COUNT; ++index) {
		// Check that no futex for given uaddress exists
		if (futex_list[index].uaddress == uaddress) {
			*out = &futex_list[index];
			return 1;
		}
	}
	// Futex was not found; return an error
	*out = NULL;
	return -1;
};

// Mark the wait node slot of a futex unused
void free_waite_node(struct wait_node *wn) {
	//printf("Free Node\n");
	wn->descr = 0;
	wn->slot_is_used = 0;
}

// Mark the slot unsued; Reset address so no false positives are possible.
void free_futex(struct futex *f){
	//printf("Free Futex\n");
	f->slot_is_used = 0;
	f->uaddress = 0;
}

int pop_waiter(struct futex *f, pthread_descr* out){
	for (size_t index = 0; index < MAX_THREADS_PER_FUTEX; ++index) {
		if (0 != f->wait_queue[index].slot_is_used) {
			pthread_descr waiting = f->wait_queue[index].descr;
			f->wait_queue[index].slot_is_used = 0;
			f->wait_queue[index].descr = 0;
			//printf("Thread %p; tsc:%llu: Remove waiter %lu from list\n",thread_self(), __builtin_ia32_rdtsc() ,index);
			*out = waiting;
			return 1;
		}
	}
	// No slot used?!
	//printf("Thread %p; tsc;%llu: Nothing to free...\n", thread_self(), __builtin_ia32_rdtsc());
	return 0;
}

/*
 * Put the current thread on the sleep queue of the futex at address
 * ``uaddr''.  Let it sleep for the specified ``timeout'' time, or
 * indefinitely if the argument is NULL.
 */
int
futex_wait(uint64_t *uaddr, uint64_t val, const struct timespec *timeout,
	int flags)
{
	__sl_acquire(&global_futex_sp);
	//__sync_synchronize();
	// struct proc *p = curproc;
	struct futex *f;
	// uint64_t nsecs = INFSLP;
	int error = -1;
	/*
	 * After reading the value a race is still possible but
	 * we deal with it by serializing all futex syscalls.
	 */
	/* If the value changed, stop here. */
	uint64_t val2 = __atomic_load_n(uaddr, __ATOMIC_SEQ_CST); 
	if ((val2 != val)) {
		__sl_release(&global_futex_sp);
		return EAGAIN;
	}

	pthread_descr t_descr = thread_self();

	//__sync_synchronize();
	register_futex(uaddr, &f);
	if(f == NULL) {
		__sl_release(&global_futex_sp);
		return EAGAIN;
	} 	
	// if ((find_wait_node(f, t_descr)) != NULL) {
	// 	DEBUG_MSG("Already queued!");
	// }

	// if (timeout != NULL) {
	// TODO: Timeout stuff
	// 	struct timespec ts;

	// 	if ((error = copyin(timeout, &ts, sizeof(ts))))
	// 		return error;
	// 	if (ts.tv_sec < 0 || !timespecisvalid(&ts))
	// 		return EINVAL;
	// 	nsecs = MAX(1, MIN(TIMESPEC_TO_NSEC(&ts), MAXTSLP));
	// }

	__sync_synchronize();
	// printf("Thread %p; tsc: %llu; futex_WAIT\n", thread_self(), __builtin_ia32_rdtsc());
	register_waiter(f, t_descr);
	//__sync_synchronize();

	error = 1;

	// // TODO: Do a timed sleep or not.
	// // error = rwsleep_nsec(p, &ftlock, PWAIT|PCATCH, "fsleep", nsecs);
	// // if (error == ERESTART)
	// // 	error = ECANCELED;
	// // else if (error == EWOULDBLOCK) {
	// // 	/* A race occurred between a wakeup and a timeout. */
	// // 	if (p->p_futex == NULL)
	// // 		error = 0;
	// // 	else
	// // 		error = ETIMEDOUT;
	// // }

	// if (f->queue_dirty) {
	// 	// We've been removed -> someone wanted us to wake up
	// 	if (find_wait_node(f, t_descr) == NULL) {
	// 		f->queue_dirty = 0;
	// 		__sl_release(&global_futex_sp);
	// 		return EAGAIN;
	// 	}
	// }
	__sync_synchronize();
	__sl_release(&global_futex_sp);
	suspend(t_descr);



	// // We regain control. This is either because we have been woken or because 
	// // The time timed out. We should check if we were removed from the queue and
	// // if not, we should do this on our own

	/* Remove ourself if we haven't been awaken by another thread. */
	//__sync_synchronize();
	// if ((find_wait_node(f, t_descr)) != NULL) {
	// 	DEBUG_MSG("Woke up, found my node!");
	// 	if (-1 != remove_wait_node(f, t_descr))
	// 		DEBUG_MSG("Really did remove the node...");

	// 	//__sync_synchronize();
	// 	__sl_release(&global_futex_sp);
	// 	return EAGAIN;
	// }
	//__sync_synchronize();
	return error;
}

/*
 * Wakeup at most ``n'' sibling threads sleeping on a futex at address
 * ``uaddr'' and requeue at most ``m'' sibling threads on a futex at
 * address ``uaddr2''.
 */
unsigned long 
futex_requeue(uint64_t *uaddr, uint64_t n, uint64_t *uaddr2, uint64_t m,
	int flags)
{
	__sl_acquire(&global_futex_sp);
	struct futex *f = NULL, *g = NULL;
	unsigned long count = 0;

	find_futex(uaddr, &f);

	if (f == NULL) {
		// //printf("Wake. FUTEX NULL!\n");
		__sl_release(&global_futex_sp);
		return 0;
	}

	// current = f->sleep_queue_head;
	// if (NULL != current) {
	// 	remove_wait_node(f, current->thr);
	// 	++count;
	// }

	__sync_synchronize();
	// uint64_t val2 = __atomic_load_n(uaddr, __ATOMIC_SEQ_CST); 
	// if ((current == NULL) ) {
	// 	//printf("Empty queue!\n");
	// 	if(val2 != 0)
	// 		//printf("But waiters...\n");
	// }
	struct wait_node* head = NULL;
	pthread_descr thr;
	for (int result = pop_waiter(f, &thr); 0 != result; result = pop_waiter(f, &thr)) {
		if (1/*count < n*/) {
			// //printf("count=%d\n", count);
			// //printf("Pointer of current %p\n", current);
			// //printf("Derefed, tID=%llu\n", thread);
			// next = current->next;
			// //printf("Pointer of next %p\n", next);
			// printf("Wake...%p\n", thr);
			//__sync_synchronize();
			restart(thr);
			//__sync_synchronize();
			// //printf("Did restart...%lu\n", thread);

			// if(next == current) {
			// 	DEBUG_MSG("Just remvoed the head. Better we stop");
			// 	break;
			// }
		} else if (uaddr2 != NULL) {
			// //printf("Woanders\n");
			register_futex(uaddr2, &g);
			register_waiter(g, thr);
		}
		// current = next;
		++count;
		// if (downs != ups)
		// 	//printf("ups != downs\n");
	}
	__sync_synchronize();
	__sl_release(&global_futex_sp);
	return count;
}

/*
 * Wakeup at most ``n'' sibling threads sleeping on a futex at address
 * ``uaddr''.
 */
int
futex_wake(uint64_t *uaddr, uint64_t n, int flags)
{
	return futex_requeue(uaddr, n, NULL, 0, flags);
}
