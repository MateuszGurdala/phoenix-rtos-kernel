#include "include/monitor.h"
#include "lib/lib.h"
#include "proc/msg.h"
#include "proc/threads.h"
#include "monitorpriv.h"

static struct {
#define MBUFF(NAME, TYPE, SIZE) m_buffer NAME;
	MBUFFERS()
#undef MBUFF
} monitor_buffers;

static struct {
	unsigned port;

	struct m_queue_utils rtq;
	m_data *mdata_q;

	struct m_queue_utils odq;
	m_buffer *mbuffer_q;
} monitor_common;

int monitor_set_port(unsigned port)
{
	lib_printf("monitor: initialized server port\n");
	monitor_common.port = port;
	return EOK;
}

m_buffer *
_access_mbuffer(unsigned ebuff)
{
	switch (ebuff) {
#define MBUFF(NAME, TYPE, SIZE) \
	case mbuff_##NAME: return &monitor_buffers.NAME;
		MBUFFERS()
#undef MBUFF
		default: return NULL;
	}
}

int _mbuff_init(unsigned ebuff, unsigned data_type, unsigned size)
{
	m_buffer *mbuffer = _access_mbuffer(ebuff);

	// Check init data
	if (mbuffer == NULL) {
		return -ENOBUFF;
	}

	if (data_type != DT_ONDEMAND && data_type != DT_REALTIME) {
		return -ENODT;
	}

	// Memory allocation
	if ((mbuffer->buffer = vm_kmalloc(sizeof(m_data) * size)) == NULL) {
		return -ENOMEM;
	}

	// mbuffer metadata
	mbuffer->data_type = data_type;
	mbuffer->size = 0;

	switch (data_type) {
		case DT_ONDEMAND:
			// Buffer size must be at least 1
			if (size == 0) {
				return -EBUFFSIZE;
			}
			mbuffer->max_size = size;

			break;
		case DT_REALTIME:
			vm_kfree(mbuffer->buffer);
			mbuffer->max_size = 0;
			break;
		default:
			break;
	}
	return EOK;
}

int monitor_sendsrv_mbuffer(m_buffer *mbuffer)
{
	int err = EOK;
	msg_t msg = {
		.type = monOnDemandData,

		// Server throws segmentation fault when entire m_buffer struct is passed
		.i.data = mbuffer->buffer,
		.i.size = sizeof(m_data) * mbuffer->size,

		// mbuffer metadata
		.i.mbuff_data.size = mbuffer->size,
		.i.mbuff_data.id = mbuffer->id,

		// Response is unnecessary
		.o.data = NULL,
		.o.size = 0
	};

	if (mbuffer != NULL) {
		err = proc_send(monitor_common.port, &msg);

		// Free buffer
		vm_kfree(mbuffer->buffer);
	}
	return err;
}

int monitor_sendsrv_mdata(m_data *mdata)
{
	int err = EOK;
	msg_t msg = {
		.type = monRealTimeData,
		.i.data = mdata,
		.i.size = sizeof(m_data),
		.o.data = NULL,
		.o.size = 0
	};

	err = proc_send(monitor_common.port, &msg);
	return err;
}

int _monitor_queue_mbuffer(m_buffer *mbuffer)
{
	monitor_common.mbuffer_q[monitor_common.odq.queue] = *mbuffer;
	++monitor_common.odq.queue;

	if (monitor_common.odq.queue == ODQ_MAXSIZE) {
		for (unsigned i = 0; i < monitor_common.odq.queue; ++i) {
			vm_kfree(monitor_common.mbuffer_q[i].buffer);
		}
		monitor_common.odq.queue = 0;
	}

	return EOK;
}

int monitor_queue_mbuffer(m_buffer *mbuffer)
{
	proc_lockSet(&monitor_common.odq.lock);
	_monitor_queue_mbuffer(mbuffer);
	proc_lockClear(&monitor_common.odq.lock);
	return EOK;
}

int _monitor_queue_mdata(m_data *mdata)
{
	monitor_common.mdata_q[monitor_common.rtq.queue] = *mdata;
	++monitor_common.rtq.queue;
	monitor_common.rtq.queue %= RTQ_MAXSIZE;

	return EOK;
}

int monitor_queue_mdata(m_data *mdata)
{
	proc_lockSet(&monitor_common.rtq.lock);
	_monitor_queue_mdata(mdata);
	proc_lockClear(&monitor_common.rtq.lock);

	return EOK;
}

int monitor_demand_data(char *file_name)
{
	int err = EOK;
	msg_t msg = {
		.type = monReadOnDemandData,
		.i.data = NULL,
		.i.size = 0,
		.o.data = NULL,
		.o.size = 0
	};

	hal_strcpy(&msg.i.raw, file_name);

	err = proc_send(monitor_common.port, &msg);
	return err;
}

int monitor_save_data(unsigned ebuff, m_data mdata)
{
	int err;
	m_buffer *mbuffer;

	// Add metadata
	mdata.timestamp = hal_timerGetUs();

	if ((mbuffer = _access_mbuffer(ebuff)) != NULL) {
		switch (mbuffer->data_type) {
			case DT_ONDEMAND:
				mbuffer->buffer[mbuffer->size] = mdata;
				mbuffer->size++;

				if (mbuffer->size == mbuffer->max_size) {
					if ((err = monitor_queue_mbuffer(mbuffer)) < 0) {
						return err;
					}
					mbuffer->size = 0;

					// Allocate new buffer without free, previous data is still necessary
					if ((mbuffer->buffer = vm_kmalloc(sizeof(m_data) * mbuffer->max_size)) == NULL) {
						return -ENOMEM;
					}
				}
				break;
			case DT_REALTIME:
				return monitor_queue_mdata(&mdata);
				break;
			default:
				return -ENODT;
				break;
		}
	}
	else {
		return -ENOBUFF;
	}

	return EOK;
}

void monitor_dqthr()
{
	unsigned tmpsize = 0;
	void *tmp = NULL;
	void *buffcpy = NULL;

	lib_printf("monitor: spawning data queue thread\n");

	if (!monitor_common.port) {
		proc_threadSleep(DQTHR_INIT_SLEEP);  // Scheduling must be enabled before locks can be used
	}

	for (;;) {
		if (monitor_common.rtq.queue) {
			proc_lockSet(&monitor_common.rtq.lock);

			for (unsigned i = 0; i < monitor_common.rtq.queue; ++i) {
				monitor_sendsrv_mdata(&monitor_common.mdata_q[i]);
			}
			monitor_common.rtq.queue = 0;

			proc_lockClear(&monitor_common.rtq.lock);
		}

		if (monitor_common.odq.queue) {
			proc_lockSet(&monitor_common.odq.lock);

			for (unsigned i = 0; i < monitor_common.odq.queue; ++i) {
				monitor_sendsrv_mbuffer(&monitor_common.mbuffer_q[i]);
			}
			monitor_common.odq.queue = 0;

			proc_lockClear(&monitor_common.odq.lock);
		}

		proc_threadSleep(DQTHR_SLEEP);
	}
}

void _monitor_init()
{
	int err = 0;

	proc_lockInit(&monitor_common.rtq.lock, "monitor.rtq.lock");
	proc_lockInit(&monitor_common.odq.lock, "monitor.odq.lock");

	monitor_common.rtq.queue = 0;
	monitor_common.mdata_q = vm_kmalloc(sizeof(m_data) * RTQ_MAXSIZE);

	monitor_common.odq.queue = 0;
	monitor_common.mbuffer_q = vm_kmalloc(sizeof(m_buffer) * ODQ_MAXSIZE);

	monitor_common.port = 0;

	lib_printf("monitor: init\n");
#define MBUFF(NAME, TYPE, SIZE) err += _mbuff_init(mbuff_##NAME, TYPE, SIZE);
	MBUFFERS()
#undef MBUFF

	if (err < 0) {
		lib_printf("monitor: init failed\n");
	}
	else {
		lib_printf("monitor: init success\n");
	}
}