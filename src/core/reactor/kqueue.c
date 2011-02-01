/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */ 


#ifdef HAVE_KQUEUE
#include "general.h"
#include "reactor.h"

int kqueue_init(io_wait_h *h)
{
	h->kq_fd = kqueue();

	if (h->kq_fd == -1)
	{
		LM_ERR("kqueue: %s [%d]\n", strerror(errno), errno);
		return -1;
	}

	return 0;

}

void kqueue_destroy(io_wait_h *h)
{
	if (h->kq_fd != -1)
	{
		close(h->kq_fd);
		h->kq_fd = -1;
	}
}

int kqueue_loop(io_wait_h *h, int t)
{

	struct fd_map e;
	int n, r;
	struct timespec tspec;
	reactor_t * rec = (reactor_t *) h->arg;

	tspec.tv_sec = t;
	tspec.tv_nsec = 0;


again:
	n = kevent(h->kq_fd, NULL, 0, h->kq_array, h->max_fd_no, &tspec);

	if (n == -1)
	{
		if (errno == EINTR) goto again; /* signal, ignore it */
		else
		{
			LM_ERR("kevent: %s [%d]\n", strerror(errno), errno);
			goto error;
		}
	}


	for (r = 0; r < n; r++)
	{
#ifdef EXTRA_DEBUG
		LM_DBG("event %d/%d: fd=%d, udata=%lx, flags=0x%x\n",
			r, n, h->kq_array[r].ident, (long) h->kq_array[r].udata,
			h->kq_array[r].flags);
#endif

		if (h->kq_array[r].flags & EV_ERROR)
		{
			LM_ERR("kevent error on fd %u: %s [%ld]\n",
				(unsigned int) h->kq_array[r].ident,
				strerror(h->kq_array[r].data),
				(long) h->kq_array[r].data);

		} else /* READ/EOF */
		{
			e = *(struct fd_map*) h->kq_array[r].udata;
			kqueue_del(h, e.fd);
			put_task(rec->disp, e);

		}
	}


error:
	return n;

}

int kqueue_del(io_wait_h *h, int fd)
{

	int n;
	struct kevent ev;
	struct timespec tspec;

	tspec.tv_sec = 0;
	tspec.tv_nsec = 0;

	if (safe_remove_from_hash(h, fd))
		goto error;

	EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

	n = kevent(h->kq_fd, &ev, 1, NULL, 0, &tspec);

	if (n == -1)
	{
		LM_ERR("kevent failed: %s [%d]\n", strerror(errno), errno);
		goto error;
	}

	return 0;
error:
	return -1;

}

int kqueue_add(io_wait_h* h, int fd, int type, int priority,
			   fd_callback *cb, void *cb_param)
{
	int n;
	struct fd_map * e;
	int filters = 0;
	struct kevent ev;
	struct timespec tspec;

	tspec.tv_sec = 0;
	tspec.tv_nsec = 0;

	if ((e = safe_add_to_hash(h, fd, type, priority, cb, cb_param)) == NULL)
		goto error;

	if (h->type == REACTOR_IN)
	{
		filters = EVFILT_READ;
	}

	if (h->type == REACTOR_OUT)
	{
		filters = EVFILT_WRITE;
	}


	EV_SET(&ev, fd, filters, EV_ADD | EV_ENABLE, 0, 0, KEV_UDATA_CAST e);

	n = kevent(h->kq_fd, &ev, 1, NULL, 0, &tspec);

	if (n == -1)
	{
		LM_ERR("kevent failed: %s [%d]\n", strerror(errno), errno);
		goto error;
	}

	return 0;
error:
	return -1;

}
#endif
