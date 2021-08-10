#ifndef EVENT_H
#define EVENT_H
#include <asm/lib/spinlock.h>

struct sched_event {
	spinlock_t lock;
	int8_t nqueued;
	struct thread_object* waiting_thread;
};

void init_event(struct sched_event *event);
void reset_event(struct sched_event *event);
void wait_event(struct sched_event *event);
void signal_event(struct sched_event *event);

#endif /* EVENT_H */
