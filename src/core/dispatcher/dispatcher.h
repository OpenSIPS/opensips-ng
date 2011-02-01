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
 *  2010-03-xx  created (adragus)
 */


#include "../reactor/fd_map.h"
#include "../locking/locking.h"
#include <semaphore.h>


#ifndef _CORE_DISPATCHER_H
#define _CORE_DISPATCHER_H

#define MAX_TASKS   (1<<11)

#define TASK_PRIO_READ_IO       1
#define TASK_PRIO_RESUME_IO     2
#define TASK_PRIO_RESUME_EXEC   3

typedef struct fd_map heap_node_t;

typedef struct _dispacther
{
	gen_lock_t* lock;
	sem_t sem;
	int size;
	heap_node_t nodes[MAX_TASKS];

}dispatcher_t;

/* probably implemented as a heap */
dispatcher_t* new_dispatcher();

void destroy_dispatcher(dispatcher_t* disp);

/* will be called by the reactor thread and timer thread*/
void put_task(dispatcher_t* d, heap_node_t n);

/* will be called by the worker threads */
void get_task(dispatcher_t* d, heap_node_t *n);

void inline put_task_simple(dispatcher_t* d, int priority,
		fd_callback *cb, void* cb_param);


#endif
