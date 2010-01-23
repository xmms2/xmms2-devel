#include "common.h"

#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>

#include <errno.h>

xmmsc_result_t *
setup_shm_prepare (xmmsc_connection_t *c, int32_t vv)
{
	xmmsc_result_t *res;
	xmmsc_vischunk_t *buffer;
	xmmsc_vis_unixshm_t *t;
	xmmsc_visualization_t *v;
	char shmidstr[32];

	x_check_conn (c, 0);
	v = get_dataset (c, vv);

	t = &v->transport.shm;

	/* prepare unixshm + semaphores */
	/* following access modifiers imply everyone on the system could inject wrong vis data ;) */
	t->shmid = shmget (IPC_PRIVATE, sizeof (xmmsc_vischunk_t) * XMMS_VISPACKET_SHMCOUNT, S_IRWXU + S_IRWXG + S_IRWXO);
	if (t->shmid == -1) {
		c->error = strdup ("Couldn't create the shared memory!");
		return false;
	}
	/* attach early, so that the server doesn't think we aren't there */
	buffer = shmat(t->shmid, NULL, SHM_RDONLY);
	t->buffer = buffer;

	/* we send it as string to make it work on 64bit systems.
	   Ugly? Yes, but works. */
	snprintf (shmidstr, sizeof (shmidstr), "%d", t->shmid);

	/* send packet */
	res = xmmsc_send_cmd (c, XMMS_IPC_OBJECT_VISUALIZATION,
	                      XMMS_IPC_CMD_VISUALIZATION_INIT_SHM,
	                      XMMSV_LIST_ENTRY_INT (v->id),
	                      XMMSV_LIST_ENTRY_STR (shmidstr),
	                      XMMSV_LIST_END);

	if (res) {
		xmmsc_result_visc_set (res, v);
	}

	return res;
}

bool
setup_shm_handle (xmmsc_result_t *res)
{
	bool ret;
	xmmsc_visualization_t *visc;
	xmmsc_vis_unixshm_t *t;

	visc = xmmsc_result_visc_get (res);
	if (!visc) {
		x_api_error_if (1, "non vis result?", -1);
	}

	t = &visc->transport.shm;

	if (!xmmsc_result_iserror (res)) {
		xmmsv_t *val;
		t->size = XMMS_VISPACKET_SHMCOUNT;
		t->pos = 0;
		val = xmmsc_result_get_value (res);
		xmmsv_get_int (val, &t->semid);
		ret = true;
	} else {
		/* didn't work, detach from shm to get it removed later on */
		shmdt (t->buffer);
		ret = false;
	}
	/* In either case, mark the shared memory segment to be destroyed.
	   The segment will only actually be destroyed after the last process detaches it. */
	shmctl (t->shmid, IPC_RMID, NULL);
	return ret;
}

void
cleanup_shm (xmmsc_vis_unixshm_t *t)
{
	shmdt (t->buffer);
}

/**
 * Decrements the client's semaphor (to read the next available chunk)
 */
static int
decrement_client (xmmsc_vis_unixshm_t *t, unsigned int blocking) {
	/* alter semaphore 1 by -1, no flags */
	struct sembuf op = { 1, -1, 0 };
	struct timespec time;
	time.tv_sec = blocking / 1000;
	time.tv_nsec = (blocking % 1000) * 1000000;
	if (semtimedop (t->semid, &op, 1, &time) == -1) {
		switch (errno) {
		case EAGAIN:
		case EINTR:
			return 0;
		case EINVAL:
		case EIDRM:
			return -1;
		default:
			perror ("Unexpected semaphore problem");
			return -1;
		}
	}
	return 1;
}

/**
 * Increments the server's semaphor (after a chunk was read)
 */
static void
increment_server (xmmsc_vis_unixshm_t *t) {
	/* alter semaphore 0 by 1, no flags */
	struct sembuf op = { 0, +1, 0 };

	if (semop (t->semid, &op, 1) == -1) {
		/* there should not occur any error */
	}
}

static int
read_start_shm (xmmsc_vis_unixshm_t *t, unsigned int blocking, xmmsc_vischunk_t **dest)
{
	int decr = decrement_client (t, blocking);
	if (decr == 1) {
		*dest = &t->buffer[t->pos];
	}
	return decr;
}

static void
read_finish_shm (xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t *dest) {
	t->pos = (t->pos + 1) % t->size;
	increment_server (t);
}

int
read_do_shm (xmmsc_vis_unixshm_t *t, xmmsc_visualization_t *v, short *buffer, int drawtime, unsigned int blocking)
{
	int old;
	int ret;
	xmmsc_vischunk_t *src;
	int i, size;

	ret = read_start_shm (t, blocking, &src);
	if (ret < 1) {
		return ret;
	}

	old = check_drawtime (net2ts (src->timestamp), drawtime);

	if (!old) {
		size = ntohs (src->size);
		for (i = 0; i < size; ++i) {
			buffer[i] = (int16_t)ntohs (src->data[i]);
		}
	}
	read_finish_shm (t, src);
	if (!old) {
		return size;
	}
	return 0;
}
