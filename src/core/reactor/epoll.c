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


#ifdef HAVE_EPOLL
#include "general.h"
#include "reactor.h"



int epoll_init(io_wait_h *h)
{

	struct epoll_event ep_event;
	struct fd_map * e;

	h->ep_array = shm_malloc(sizeof (*(h->ep_array)) * h->max_fd_no);
	if (h->ep_array == 0)
	{
		LM_CRIT("could not alloc epoll array\n");
		goto error;
	}
	memset((void*) h->ep_array, 0, sizeof (*(h->ep_array)) * h->max_fd_no);

again:
	h->epfd = epoll_create(h->max_fd_no);
	if (h->epfd == -1)
	{
		if (errno == EINTR) goto again;
		LM_ERR("epoll_create: %s [%d]\n", strerror(errno), errno);
		goto error;
	}

	if ((e = safe_add_to_hash(h, h->control_pipe[0], 0, 0, NULL, NULL)) == NULL)
		goto error;

	ep_event.events = EPOLLIN;
	ep_event.data.ptr = e;

	if( epoll_ctl(h->epfd, EPOLL_CTL_ADD, h->control_pipe[0], &ep_event) )
	{
		LM_ERR("Unable to add control pipe\n");
		goto error;
	}


	LM_DBG("Epoll init succesfull\n");
	return 0;
error:
	return -1;
}

void epoll_destroy(io_wait_h *h)
{
	if (h->epfd != -1)
	{
		close(h->epfd);
		h->epfd = -1;
	}
}

int epoll_loop(io_wait_h *h, int t)
{
	int n, r, i;
	reactor_t * rec = (reactor_t *) h->arg;
	int buff[1<<12];

again:
	LM_DBG("Waiting in epoll\n");
	n = epoll_wait(h->epfd, h->ep_array, h->max_fd_no, t * 1000);
	LM_DBG("Woke up from epoll type = %d with n= %d \n", n, h->type);

	if (n == -1)
	{
		if (errno == EINTR) goto again; /* signal, ignore it */
		else
		{
			LM_ERR("epoll_wait(%d, %p, %d, %d): %s [%d]\n",
				h->epfd, h->ep_array, h->fd_no, t * 1000,
				strerror(errno), errno);
			goto error;
		}
	}
#if 0
	if (n > 0)
	{
		for (r = 0; r < n; r++)
		{
			LM_ERR("ep_array[%d]= %x, %p\n",
				r, h->ep_array[r].events, h->ep_array[r].data.ptr);
		}
	}
#endif

	for (r = 0; r < n; r++)
	{
		struct fd_map e,ctrl;
		int len;
		
		ctrl = *(struct fd_map*) h->ep_array[r].data.ptr;

		if( ctrl.fd == h->control_pipe[0] && (h->ep_array[r].events & EPOLLIN ) )
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


				if( op == FIRE_FD )
				{
					e = *get_fd_map(h,fd);
					if( e.fd != -1 )
					{
						LM_DBG("Firing event on fd =%d\n",fd);
						if( !epoll_del(h,fd) )
							put_task(rec->disp, e);
					}
				}
				else
				{
					LM_ERR("epoll received (%d,%d,%d) from control pipe\n",
						 op, fd, ev );
				}
				
			}
		}
	}


	for (r = 0; r < n; r++)
	{
		struct fd_map e;
		
		e = *(struct fd_map*) h->ep_array[r].data.ptr;

		if (
		(h->ep_array[r].events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLOUT) )
			)
		{
			if( (e.fd != -1) && ( e.fd != h->control_pipe[0] ) )
			{
				if( !epoll_del(h,e.fd) )
					put_task(rec->disp, e);
			}

		} else
		{
			e = *(struct fd_map*) h->ep_array[r].data.ptr;
			LM_ERR("unexpected event %x on fd = %d, data=%p\n",
				h->ep_array[r].events, e.fd, h->ep_array[r].data.ptr);
		}
	}


error:
	return n;

}

int epoll_del(io_wait_h *h, int fd)
{

	int n;
	struct epoll_event ep_event;

	if (safe_remove_from_hash(h, fd))
		goto error;

	LM_DBG("epfd=%d, fd=%d\n",h->epfd,fd);
	n = epoll_ctl(h->epfd, EPOLL_CTL_DEL, fd, &ep_event);
	if (n == -1 && errno != EBADF)
	{
		LM_ERR("removing fd %d from epoll "
			"list failed: %s [%d]\n", fd,strerror(errno), errno);
		goto error;
	}

	return 0;
error:
	return -1;
}

int epoll_add(io_wait_h* h, int fd, int type, int priority,
								fd_callback *cb, void *cb_param)
{

	struct epoll_event ep_event;
	int n;
	struct fd_map * e;

	if ((e = safe_add_to_hash(h, fd, type, priority, cb, cb_param)) == NULL)
		goto error;

	
	if( h->type == REACTOR_IN)
	{
		ep_event.events = EPOLLIN;
	}
	
	if( h->type == REACTOR_OUT )
	{
		ep_event.events = EPOLLOUT;
	}

	ep_event.data.ptr = e;

again1:
	LM_DBG("epfd=%d, fd=%d\n",h->epfd,fd);
	n = epoll_ctl(h->epfd, EPOLL_CTL_ADD, fd, &ep_event);

	if (n == -1)
	{
		if (errno == EAGAIN) goto again1;
		LM_ERR("epoll_ctl failed: %s [%d]\n",
			strerror(errno), errno);
		goto error;
	}

	return 0;
error:
	return -1;

}
#endif
