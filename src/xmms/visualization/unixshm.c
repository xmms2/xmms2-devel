/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"

#ifdef _SEM_SEMUN_UNDEFINED
	union semun {
	   int              val;    /* Value for SETVAL */
	   struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	   unsigned short  *array;  /* Array for GETALL, SETALL */
	   struct seminfo  *__buf;  /* Buffer for IPC_INFO (Linux specific) */
	};
#endif

int32_t
init_shm (xmms_visualization_t *vis, int32_t id, int32_t shmid, xmms_error_t *err)
{
	struct shmid_ds shm_desc;
	int32_t semid;
	void *buffer;
	int size;
	xmms_vis_client_t *c;
	xmmsc_vis_unixshm_t *t;
	union semun semopts;

	x_fetch_client (id);

	/* MR. DEBUG */
	/*	xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "lame, more lame, shm!");
		x_release_client ();
		return -1; */


	/* test the shm */
	buffer = shmat (shmid, NULL, 0);
	if (buffer == (void*)-1) {
		xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "couldn't attach to shared memory");
		x_release_client ();
		return -1;
	}
	shmctl (shmid, IPC_STAT, &shm_desc);
	size = shm_desc.shm_segsz / sizeof (xmmsc_vischunk_t);

	/* setup the semaphore set */
	semid = semget (IPC_PRIVATE, 2, S_IRWXU + S_IRWXG + S_IRWXO);
	if (semid == -1) {
		xmms_error_set (err, XMMS_ERROR_NO_SAUSAGE, "couldn't create semaphore set");
		x_release_client ();
		return -1;
	}

	/* initially set semaphores - nothing to read, buffersize to write
	   first semaphore is the server semaphore */
	semopts.val = size;
	semctl (semid, 0, SETVAL, semopts);
	semopts.val = 0;
	semctl (semid, 1, SETVAL, semopts);

	/* set up client structure */
	c->type = VIS_UNIXSHM;
	t = &c->transport.shm;
	t->semid = semid;
	t->shmid = shmid;
	t->buffer = buffer;
	t->size = size;
	t->pos = 0;

	x_release_client ();

	xmms_log_info ("Visualization client %d initialised using Unix SHM", id);
	return semid;
}

void cleanup_shm (xmmsc_vis_unixshm_t *t)
{
	shmdt (t->buffer);
	semctl (t->semid, 0, IPC_RMID, 0);
}

/**
 * Decrements the server's semaphor (to write the next chunk)
 */
static gboolean
decrement_server (xmmsc_vis_unixshm_t *t)
{
	/* alter semaphore 0 by -1, don't block */
	struct sembuf op = { 0, -1, IPC_NOWAIT };

	while (semop (t->semid, &op, 1) == -1) {
		switch (errno) {
		case EINTR:
			break;
		case EAGAIN:
			return FALSE;
		default:
			perror ("Skipping visualization package");
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Increments the client's semaphor (after a chunk was written)
 */
static void
increment_client (xmmsc_vis_unixshm_t *t)
{
	/* alter semaphore 1 by 1, no flags */
	struct sembuf op = { 1, +1, 0 };

	if (semop (t->semid, &op, 1) == -1) {
		/* there should not occur any error */
		g_error ("visualization increment_client: %s\n", strerror (errno));
	}
}

gboolean
write_shm (xmmsc_vis_unixshm_t *t, xmms_vis_client_t *c, int32_t id, struct timeval *time, int channels, int size, short *buf)
{
	xmmsc_vischunk_t *dest;
	short res;

	if (!write_start_shm (id, t, &dest))
		return FALSE;

	tv2net (dest->timestamp, time);
	dest->format = htons (c->format);
	res = fill_buffer (dest->data, &c->prop, channels, size, buf);
	dest->size = htons (res);
	write_finish_shm (id, t, dest);

	return TRUE;
}


gboolean
write_start_shm (int32_t id, xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t **dest)
{
	struct shmid_ds shm_desc;

	/* first check if the client is still there */
	if (shmctl (t->shmid, IPC_STAT, &shm_desc) == -1) {
		g_error ("Checking SHM attachments failed: %s\n", strerror (errno));
	}
	if (shm_desc.shm_nattch == 1) {
		delete_client (id);
		return FALSE;
	}
	if (shm_desc.shm_nattch != 2) {
		g_error ("Unbelievable # of SHM attachments: %lu\n",
		         (unsigned long) shm_desc.shm_nattch);
	}

	if (!decrement_server (t)) {
		return FALSE;
	}

	*dest = &t->buffer[t->pos];
	return TRUE;
}

void
write_finish_shm (int32_t id, xmmsc_vis_unixshm_t *t, xmmsc_vischunk_t *dest)
{
	t->pos = (t->pos + 1) % t->size;
	increment_client (t);
}
