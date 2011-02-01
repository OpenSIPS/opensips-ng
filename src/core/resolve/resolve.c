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
 *  2010-07-xx  created (adragus)
 */

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <stdlib.h>

#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "resolve.h"
#include "../log.h"
#include "../utils.h"
#include "../globals.h"
#include "../net/ip_addr.h"

#include "../context.h"
#include "../locking/locking.h"
#include "../reactor/reactor.h"
#include "../dispatcher/dispatcher.h"
#include "../timer.h"


#define DNS_MAX_FD 65536
#define TIMER_FREQUENCY 5

enum
{
	IN_USED_BY_ARES = 1 << 0,
	IN_INSIDE_REACTOR = 1 << 1,
	OUT_USED_BY_ARES = 1 << 2,
	OUT_INSIDE_REACTOR = 1 << 3
};

ares_channel channel;
gen_lock_t ares_lock;
int dns_try_ipv6 = 0;

unsigned char fd_state[DNS_MAX_FD];

void reactor_to_ares(reactor_t * rec, int fd, void * param);

int timeout_func(void * param)
{
	//LM_DBG("DNS timer \n");
	lock_get(&ares_lock);

	ares_process_fd(channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);

	lock_release(&ares_lock);

	return 0;
};

/* callback called by the ares library when a socket changes state */
void socket_change_callback(void *data, ares_socket_t fd,
							int readable, int writable)
{
	LM_DBG("DNS socket change fd = %d read = %d write = %d state = %d\n",
			fd, readable, writable, (int) fd_state[fd]);

	if (readable)
	{
		fd_state[fd] |= IN_USED_BY_ARES;
		if (!(fd_state[fd] & IN_INSIDE_REACTOR))
		{
			LM_DBG("submiting to reactor\n");
			submit_task(reactor_in, (fd_callback*) reactor_to_ares, NULL,
					TASK_PRIO_READ_IO, fd, CALLBACK_COMPLEX_F);
			fd_state[fd] |= IN_INSIDE_REACTOR;
		}
	} else
	{
		fd_state[fd] &= ~IN_USED_BY_ARES;
	}

	if (writable)
	{
		fd_state[fd] |= OUT_USED_BY_ARES;
		if (!(fd_state[fd] & OUT_INSIDE_REACTOR))
		{
			submit_task(reactor_out, (fd_callback*) reactor_to_ares, NULL, 
					 TASK_PRIO_READ_IO, fd, CALLBACK_COMPLEX_F);
			fd_state[fd] |= OUT_INSIDE_REACTOR;
		}
	} else
	{
		fd_state[fd] &= ~OUT_USED_BY_ARES;
	}

}


int resolv_init(void)
{
	struct ares_options opt;
	int ret;

	lock_init(&ares_lock);

	memset(fd_state, 0, sizeof (fd_state));

	opt.sock_state_cb = socket_change_callback;
	opt.lookups = "fb";

	ret = ares_init_options(&channel, &opt, ARES_OPT_SOCK_STATE_CB | ARES_OPT_LOOKUPS );

	register_timer(timeout_func, NULL, TIMER_FREQUENCY);

	if (ret != ARES_SUCCESS)
	{
		LM_ERR("Error initializing ares:[%d]\n", ret);
	}

	return ret;
}

/* Method called by the reactor.
 * It deals with the fd state an then calls ares_process,
 * and then resubmits to the reactor if the fd is still used.
 */
void reactor_to_ares(reactor_t * rec, int fd, void * param)
{

	LM_DBG("woke up from reactor fd = %d\n", fd);
	lock_get(&ares_lock);

	if (rec->type == REACTOR_IN)
	{
		fd_state[fd] &= ~IN_INSIDE_REACTOR;

		ares_process_fd(channel, fd, ARES_SOCKET_BAD);

		if ((fd_state[fd] & IN_USED_BY_ARES) && !(fd_state[fd] & IN_INSIDE_REACTOR))
		{
			submit_task(rec, (fd_callback*) reactor_to_ares, NULL,
					 TASK_PRIO_READ_IO, fd, CALLBACK_COMPLEX_F);
			fd_state[fd] |= IN_INSIDE_REACTOR;
		}

	} else
	{

		fd_state[fd] &= ~OUT_INSIDE_REACTOR;

		ares_process_fd(channel, ARES_SOCKET_BAD, fd);

		if ((fd_state[fd] & OUT_USED_BY_ARES) && !(fd_state[fd] & OUT_INSIDE_REACTOR))
		{
			submit_task(rec, (fd_callback*) reactor_to_ares, NULL,
					 TASK_PRIO_READ_IO, fd, CALLBACK_COMPLEX_F);
			fd_state[fd] |= OUT_INSIDE_REACTOR;
		}

	}

	lock_release(&ares_lock);

}



void free_request_list(dns_request_t * l)
{
	dns_request_t * tmp;

	while(l)
	{
		tmp = l;
		l = l->next;
		shm_free(tmp->name);
		shm_free(tmp);
	}
}

