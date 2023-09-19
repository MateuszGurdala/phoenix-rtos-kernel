#ifndef _MONITOR_PRIVATE_H
#define _MONITOR_PRIVATE_H

#define DQTHR_INIT_SLEEP 1000000  // us
#define DQTHR_SLEEP      50000    // us

struct m_queue_utils {
	unsigned queue;
	spinlock_t lock;
};

#endif