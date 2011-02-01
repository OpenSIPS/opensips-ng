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



#include <time.h>


#include <pthread.h>

#include "io_wait.h"

#include "reactor.h"
#include "../threading.h"
#include "../dispatcher/dispatcher.h"
#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "../locking/locking.h"
#include "io_wait.h"

/*
 * TODO check out the possibility of ONE-SHOT events with epoll and kqueue
 */

//TODO add control pipe to the listening kqueue
//TODO react to fire event (select,poll,kqueue,)

reactor_t * new_reactor(int type , dispatcher_t * disp)
{

	reactor_t* ret = shm_malloc(sizeof (*ret));

	if (ret == NULL)
	{
		LM_ERR("Allocating reactor\n");
		return NULL;
	}

	if( disp == NULL )
	{
		LM_ERR("Null dispatcher\n");
		return NULL;
	}

	if( type != REACTOR_IN && type != REACTOR_OUT )
	{
		LM_ERR("Invalid  type for reactor:%d\n",type);
		return NULL;
	}

	ret->disp = disp;
	ret->type = type;

	ret->io_handler = shm_malloc(sizeof (*ret->io_handler));

	if (ret->io_handler == NULL)
	{
		LM_ERR("Allocating handler\n");
		return NULL;
	}

	memset(ret->io_handler, 0, sizeof (*ret->io_handler));

	if (init_io_wait(ret, ret->io_handler, MAX_TASKS, 0, type) < 0)
	{
		LM_ERR("Initializing I/O wait handler\n");
		return NULL;
	};

	ret->io_handler->arg = ret;
	return ret;
}

void destroy_reactor(reactor_t * reactor)
{
	destroy_io_wait(reactor->io_handler);
	shm_free(reactor->io_handler);
	shm_free(reactor);
}

/* must be thread-safe */
void submit_task(reactor_t* rec, fd_callback cb, void *cb_param, int priority, int fd, int flags)
{
	io_watch_add(rec->io_handler, fd, flags, priority, cb, cb_param);
}

/* must be thread-safe */
void fire_fd(reactor_t* rec, int fd)
{
	io_watch_fire(rec->io_handler, fd);
}


void* receive_loop(void * x)
{
	reactor_t* r = (reactor_t *) x;
	io_wait_h* io_w = r->io_handler;

	while (1)
	{
		io_wait_loop(io_w, 4000);
	}

	return NULL;
}

int reactor_start(reactor_t * rec, char *name)
{
	return pt_create_thread( name, receive_loop, rec);
}
