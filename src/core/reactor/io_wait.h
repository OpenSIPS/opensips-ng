/*
 * Copyright (C) 2005 iptelorg GmbH
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
 * History:
 * --------
 *  2005-06-13  created by andrei
 *  2005-06-26  added kqueue (andrei)
 *  2005-07-01  added /dev/poll (andrei)
 */

#ifndef _io_wait_h
#define _io_wait_h

#include "general.h"
#include "select.h"
#include "poll.h"
#include "epoll.h"
#include "kqueue.h"

/************************FUNCTION SECTION******************************/

inline static int io_watch_add(io_wait_h* h,int fd, int type,
		int priority, fd_callback *cb, void *cb_param)
{
	switch (h->poll_method) {
		case POLL_SELECT:
			return select_add(h, fd, type, priority, cb, cb_param);
		case POLL_POLL:
			return poll_add(h, fd, type, priority, cb, cb_param);

		#ifdef HAVE_EPOLL
		case POLL_EPOLL:
			return epoll_add(h, fd, type, priority, cb, cb_param);
		#endif

		#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			return kqueue_add(h, fd, type, priority, cb, cb_param);
		#endif

		default:
			LM_ERR("Unrecognized poll method\n");
			return -1;
	}
}

inline static int io_watch_fire(io_wait_h* h, int fd)
{
	return send_fire(h, fd);
}

inline static int io_wait_loop(io_wait_h* h, int t)
{
	switch (h->poll_method) {
		case POLL_SELECT:
			return select_loop(h, t);
		case POLL_POLL:
			return poll_loop(h, t);

		#ifdef HAVE_EPOLL
		case POLL_EPOLL:
			return epoll_loop(h, t);
		#endif

		#ifdef HAVE_KQUEUE
		case POLL_KQUEUE:
			return kqueue_loop(h, t);
		#endif

		default:
			LM_ERR("Unrecognized poll method\n");
			return -1;


	}

}


int init_io_wait(struct _reactor *r, io_wait_h* h, int max_fd, enum poll_types poll_method, int type);


void destroy_io_wait(io_wait_h* h);


#endif
