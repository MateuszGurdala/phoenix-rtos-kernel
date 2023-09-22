#ifndef _MONITOR_H
#define _MONITOR_H

#include "mbuffers.h"

#define DT_ONDEMAND 0
#define DT_REALTIME 1

#define FILE_MAX_LENGTH 10
#define MAX_MSG_LENGTH  64

#define RTQ_MAXSIZE 512
#define ODQ_MAXSIZE 8

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
		char msg[MAX_MSG_LENGTH];

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
	char filename[FILE_MAX_LENGTH];
	m_data *buffer;
} m_buffer;

#endif