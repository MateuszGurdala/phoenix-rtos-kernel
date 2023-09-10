#ifndef _MONITOR_PRIVATE_H
#define _MONITOR_PRIVATE_H

#define RTQ_MAXSIZE 512
#define ODQ_MAXSIZE 8
#define DQTHR_INIT_SLEEP 1000000  // us
#define DQTHR_SLEEP 50000  // us

struct m_queue_utils {
	unsigned queue;
	lock_t lock;
};

extern void monitor_dqthr();

#endif