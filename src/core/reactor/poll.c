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


#include "general.h"
#include "reactor.h"

int poll_init(io_wait_h *h)
{
	array_fd_add(h, h->control_pipe[0], REACTOR_IN);
	set_fd_flags(O_NONBLOCK, h->control_pipe[0]);

	return 0;
error:
	return -1;
}

void poll_destroy(io_wait_h *h)
{
	close(h->control_pipe[0]);
	close(h->control_pipe[1]);
}

int poll_loop(io_wait_h *h, int t)
{
	int n, ret, len;
	int r, i;
	int buff[4096];
	struct fd_map e;
	reactor_t * rec = (reactor_t *) h->arg;

again:
	LM_DBG("Sleeping with %d fds\n", h->fd_no);
	ret = n = poll(h->fd_array, h->fd_no, t * 1000);
	LM_DBG("Woke up with: %d fds\n", n);

	if (n < 0)
	{
		if (errno == EINTR) goto again; /* just a signal */
		LM_ERR("poll: %s [%d]\n", strerror(errno), errno);
		n = 0;
		/* continue */
	}
	
	/*
	 * Go through the control pipe to process change events
	 */
	
	if( h->fd_array[0].revents & ( POLLERR | POLLHUP ) )
	{
		LM_ERR("Error on control pipe for poll [events =%d]\n",
			 h->fd_array[0].revents);
	}

	if (h->fd_array[0].revents & POLLIN )
	{

		if ((len = read(h->control_pipe[0], buff, sizeof (buff))) < 0)
		{
			LM_ERR("unable to read command from control pipe\n");
			return -1;
		}

		for (i = 0; i < len / sizeof (int); i += 3)
		{
			int op = buff[i];
			int fd = buff[i + 1];
			int ev = buff[i + 2];

			if (op == ADD_FD)
			{
				array_fd_add(h, fd, ev);
			}

			if( op == FIRE_FD )
			{
				e = *get_fd_map(h,fd);
				if( e.fd != -1 )
				{
					LM_DBG("Firing event on fd =%d\n",fd);
					if( !array_fd_del(h,fd,-1) )
						put_task(rec->disp, e);
				}
			}

		}

	}

	/*
	 * Process all the events,
	 * except the first one which is the control pipe
	 */

	r = 1;
	while (n > 0 && r < h->fd_no)
	{
		if (h->fd_array[r].revents & (POLLIN | POLLERR | POLLHUP | POLLOUT))
		{
			e = *get_fd_map(h, h->fd_array[r].fd);
			array_fd_del(h, h->fd_array[r].fd, r);
			put_task(rec->disp, e);
			n--;

		} else
			r++;
	}

	return ret;

}

/*
 * poll_add just sends messages through the control pipe
 */

int poll_add(io_wait_h* h, int fd, int type, int priority,
											fd_callback cb, void *cb_param)
{

	struct fd_map* e;
	int ops[CONTROL_SIZE];

	if ((e = safe_add_to_hash(h, fd, type, priority, cb, cb_param)) == NULL)
		goto error;

	ops[0] = ADD_FD;
	ops[1] = fd;
	ops[2] = h->type;

	if (write(h->control_pipe[1], ops, sizeof (ops)) < 0)
	{
		LM_ERR("Unable to send message through control pipe\n");
		goto error;
	}

	return 0;
error:
	return -1;

}



