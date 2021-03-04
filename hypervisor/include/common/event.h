#ifndef EVENT_H
#define EVENT_H
#include <x86/lib/spinlock.h>

struct sched_event {
	spinlock_t lock;
	bool set;
	struct thread_object* waiting_thread;
};

void init_event(struct sched_event *event);
void reset_event(struct sched_event *event);
void wait_event(struct sched_event *event);
void signal_event(struct sched_event *event);

#endif /* EVENT_H */
