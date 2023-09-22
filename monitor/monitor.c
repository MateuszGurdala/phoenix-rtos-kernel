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
	spinlock_t odq_lock;

	struct m_queue_utils rtq;
	m_data mdata_q[RTQ_MAXSIZE];
} monitor_common;

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
	if (mbuffer == NULL)
		return -ENOBUFF;

	if (data_type != DT_ONDEMAND && data_type != DT_REALTIME)
		return -ENODT;

	// Memory allocation
	if ((mbuffer->buffer = vm_kmalloc(sizeof(m_data) * size)) == NULL)
		return -ENOMEM;

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

void _monitor_queue_mdata(m_data *mdata)
{
	monitor_common.mdata_q[monitor_common.rtq.queue] = *mdata;
	++monitor_common.rtq.queue;
	monitor_common.rtq.queue %= RTQ_MAXSIZE;
}

void monitor_queue_mdata(m_data *mdata)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&monitor_common.rtq.lock, &sc);
	_monitor_queue_mdata(mdata);
	hal_spinlockClear(&monitor_common.rtq.lock, &sc);
}

int _monitor_push_odq_data(unsigned ebuff, m_data *mdata)
{
	m_buffer *mbuffer;

	if ((mbuffer = _access_mbuffer(ebuff)) != NULL) {
		if (mbuffer->size != mbuffer->max_size) {
			mbuffer->buffer[mbuffer->size] = *mdata;
			mbuffer->size++;
		}
		return EOK;
	}

	return -ENOBUFF;
}

int monitor_push_odq_data(unsigned ebuff, m_data *mdata)
{
	spinlock_ctx_t sc;
	int err;

	hal_spinlockSet(&monitor_common.odq_lock, &sc);
	err = _monitor_push_odq_data(ebuff, mdata);
	hal_spinlockClear(&monitor_common.odq_lock, &sc);

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
				return monitor_push_odq_data(ebuff, &mdata);
				break;
			case DT_REALTIME:
				monitor_queue_mdata(&mdata);
				return EOK;
				break;
			default:
				return -ENODT;
				break;
		}
	}
	return -ENOBUFF;
}

int monitor_get_mdata_q(m_data *mdata_qcpy)
{
	int qtemp = 0;
	spinlock_ctx_t sc;

	if (monitor_common.rtq.queue) {
		hal_spinlockSet(&monitor_common.rtq.lock, &sc);

		qtemp = monitor_common.rtq.queue;
		hal_memcpy(mdata_qcpy, &monitor_common.mdata_q, monitor_common.rtq.queue * sizeof(m_data));
		monitor_common.rtq.queue = 0;

		hal_spinlockClear(&monitor_common.rtq.lock, &sc);
	}

	return qtemp;
}

int monitor_empty_full_mbuffer(unsigned ebuff, m_data *buff_cpy)
{
	m_buffer *mbuffer;
	spinlock_ctx_t sc;

	hal_spinlockSet(&monitor_common.odq_lock, &sc);

	if ((mbuffer = _access_mbuffer(ebuff)) == NULL) {
		hal_spinlockClear(&monitor_common.odq_lock, &sc);
		return -ENOBUFF;
	}

	if (mbuffer->data_type != DT_ONDEMAND) {
		hal_spinlockClear(&monitor_common.odq_lock, &sc);
		return -ENODT;
	}

	if (mbuffer->size != mbuffer->max_size) {
		hal_spinlockClear(&monitor_common.odq_lock, &sc);
		return -ENOBUFFULL;
	}

	hal_memcpy(buff_cpy, mbuffer->buffer, mbuffer->max_size * sizeof(m_data));
	mbuffer->size = 0;

	hal_spinlockClear(&monitor_common.odq_lock, &sc);

	return mbuffer->max_size;
}

void _monitor_init()
{
	int err = EOK;

	hal_spinlockCreate(&monitor_common.rtq.lock, "monitor.rtq.lock");
	hal_spinlockCreate(&monitor_common.odq_lock, "monitor.odq_lock");

	monitor_common.rtq.queue = 0;

	lib_printf("monitor: init\n");
#define MBUFF(NAME, TYPE, SIZE) err += _mbuff_init(mbuff_##NAME, TYPE, SIZE);
	MBUFFERS()
#undef MBUFF

	if (err < EOK)
		lib_printf("monitor: init failed\n");
	else
		lib_printf("monitor: init success\n");
}