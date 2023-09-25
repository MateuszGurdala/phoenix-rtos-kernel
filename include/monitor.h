#ifndef _MONITOR_H
#define _MONITOR_H

#include "mbuffers.h"

#define RTQ_MAXSIZE 512

#define RAW_MSG_LENGTH 48

enum E_MBUFFERS {
#define MBUFF(NAME, TYPE, SIZE) mbuff_##NAME,
	MBUFFERS()
#undef MBUFF
		mbuff_end
};

enum T_MDATA {
	mdt_msg,
	mdt_scheduleinfo
};

typedef struct
{
	unsigned mtype;
	unsigned long long timestamp;

	union {
		char msg[RAW_MSG_LENGTH];

		struct
		{
			unsigned long pid;
			unsigned long tid;
			unsigned long npid;
			unsigned long ntid;
		} schedule_info;

	} data;
} m_data;

typedef struct
{
	unsigned id;
	unsigned data_type;
	unsigned max_size;
	unsigned size;
	m_data *buffer;
} m_buffer;

#endif