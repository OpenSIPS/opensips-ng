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


int inline select_local_del(io_wait_h *h, int fd, int idx)
{
	FD_CLR(fd, &h->master_set);
	FD_CLR(fd, &h->out_set);
	return array_fd_del(h, fd, idx);

}

void inline select_local_add(io_wait_h *h, int fd, int ev)
{
	if (fd > h->max_fd_select)
		h->max_fd_select = fd;

	array_fd_add(h, fd, ev);

	if (ev == REACTOR_IN)
	{
		FD_SET(fd, &h->master_set);
	}

	if (ev == REACTOR_OUT)
	{
		FD_SET(fd, &h->out_set);
	}
}

int select_init(io_wait_h *h)
{
	FD_ZERO(&h->master_set);
	select_local_add(h, h->control_pipe[0], REACTOR_IN);
	set_fd_flags(O_NONBLOCK, h->control_pipe[0]);
	FD_ZERO(&h->out_set);

	return 0;
error:
	return -1;
}

void select_destroy(io_wait_h *h)
{
	close(h->control_pipe[0]);
	close(h->control_pipe[1]);
}


int select_loop(io_wait_h *h, int t)
{
	int n, ret,len;
	struct timeval timeout;
	int r, i;
	int buff[4096];
	fd_set local_set;
	fd_set local_out_set;
	struct fd_map e;
	reactor_t * rec = (reactor_t *) h->arg;

again:

	local_set = h->master_set;
	local_out_set = h->out_set;
	timeout.tv_sec = t;
	timeout.tv_usec = 0;

	ret = n = select(h->max_fd_select + 1, &local_set, &local_out_set,
					0, &timeout);


	if (n < 0)
	{
		if (errno == EINTR) goto again; /* just a signal */
		LM_ERR("select: %s [%d]\n", strerror(errno), errno);
		n = 0;
		/* continue */
	}

	/*
	 * Process the control pipe
	 */

	if (FD_ISSET(h->control_pipe[0], &local_set))
	{

		if ((len = read(h->control_pipe[0], buff, sizeof (buff))) < 0)
		{
			LM_ERR("unable to read command from control pipe\n");
			return -1;
		}

		for (i = 0; i < len / sizeof (int); i += CONTROL_SIZE)
		{
			int op = buff[i];
			int fd = buff[i + 1];
			int ev = buff[i + 2];

			if (op == ADD_FD)
			{
				LM_DBG("Adding fd = %d events = %d\n",fd,ev);
				select_local_add(h, fd, ev);
			}

			if( op == FIRE_FD )
			{
				e = *get_fd_map(h,fd);
				if( e.fd != -1 )
				{
					LM_DBG("Firing event on fd =%d\n",fd);
					if( !select_local_del(h,fd,-1) )
						put_task(rec->disp, e);
				}
			}
			
		}
	}

	/* use poll fd array to process all events except the first */

	r = 1;
	while (n > 0 && r < h->fd_no)
	{
		if (FD_ISSET(h->fd_array[r].fd, &local_set)
					 || FD_ISSET(h->fd_array[r].fd, &local_out_set) )
		{
			e = *get_fd_map(h, h->fd_array[r].fd);
			select_local_del(h, h->fd_array[r].fd, r);
			put_task(rec->disp, e);
			n--;

		} else
			r++;
	}

	return ret;

}

/*
 * select_add just sends messages through the control pipe
 */


int select_add(io_wait_h* h, int fd, int type, int priority,
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

