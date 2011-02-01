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


#ifndef _CORE_REACTOR_REACTOR_H
#define _CORE_REACTOR_REACTOR_H

#include "../dispatcher/dispatcher.h"
#include "../locking/locking.h"
#include "io_wait.h"

enum REACTOR_TYPES
{
	REACTOR_IN = 1,
	REACTOR_OUT = 2,
};

typedef struct _reactor
{
	int type;
	io_wait_h* io_handler;
	dispatcher_t* disp;
} reactor_t;


/* will create a thread that is listening */
reactor_t* new_reactor(int type, dispatcher_t * disp);

int reactor_start(reactor_t * rec, char *name);

void destroy_reactor(reactor_t* reactor);

dispatcher_t get_dispacther(reactor_t* reactor);

/*
 * The flags parameter can be
 * CALLBACK_COMPLEX_F, if you want the callback to be called with the fd_map structure
 *
 */
/* must be thread-safe */
void submit_task(reactor_t* rec, fd_callback cb, void *cb_param,
		int priority, int fd, int flags);

/* must be thread-safe */
void fire_fd(reactor_t* rec, int fd);

#endif
