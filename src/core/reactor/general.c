/*
 * Copyright (C) 2010 OpenSIPS Project
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * history:
 * ---------
 *  2010-04-xx  created (adragus)
 */

#include "general.h"
#include "reactor.h"

int safe_remove_from_hash(io_wait_h * h, int fd)
{
	struct fd_map * e;
	if (fd < 0 || fd > h->max_fd_no)
	{
		LM_ERR("Invalid fd : fd = %d max_fd = %d\n", fd, h->max_fd_no);
		goto error;
	}

	e = get_fd_map(h, fd);

	if (e == NULL)
	{
		LM_ERR("Cannot get fd_map from hash\n");
		goto error;
	}
	unhash_fd_map(e);

	return 0;
error:
	return -1;
}

struct fd_map * safe_add_to_hash(io_wait_h * h, int fd,
			int flags, int priority, fd_callback cb, void * cb_param)
{
	struct fd_map * e;

	if (fd < 0 || fd > h->max_fd_no)
	{
		LM_ERR("Invalid fd : fd = %d max_fd = %d\n", fd, h->max_fd_no);
		goto error;
	}

	e = hash_fd_map(h, fd, flags, priority, cb, cb_param);


	if (e == NULL)
	{
		LM_ERR("Cannot add fd_map to hash\n");
		goto error;
	}

	return e;
error:
	return NULL;
}

int inline array_fd_del(io_wait_h * h, int fd1, int idx)
{

	if (idx == -1)
	{
		/* fix idx if -1 and needed */
		for (idx = 0; (idx < h->fd_no) && (h->fd_array[idx].fd != fd1); idx++);
	}
	if (idx < h->fd_no)
	{
		memmove(&h->fd_array[idx], &h->fd_array[idx + 1],
				(h->fd_no - (idx + 1)) * sizeof (*(h->fd_array)));

		h->fd_no--;
		return 0;

	} else
	{
		return -1;
	}
}

void inline array_fd_add(io_wait_h * h, int fd1, int ev)
{
	h->fd_array[h->fd_no].fd = fd1;

	if( ev == REACTOR_IN )
	{
		h->fd_array[h->fd_no].events = POLLIN; /* useless for select */
	}
	if( ev == REACTOR_OUT )
	{
		h->fd_array[h->fd_no].events = POLLOUT; /* useless for select */
	}

	h->fd_array[h->fd_no].revents = 0; /* useless for select */
	h->fd_no++;

}

int send_fire(io_wait_h *h, int fd)
{
	int ops[CONTROL_SIZE];

	ops[0] = FIRE_FD;
	ops[1] = fd;

	if (write(h->control_pipe[1], ops, sizeof (ops)) < 0)
	{
		LM_ERR("Unable to send message through control pipe\n");
		goto error;
	}

	return 0;
error:
	return -1;

}

