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



#include "../mem/mem.h"
#include "../mem/shm_mem.h"
#include "../locking/locking.h"
#include "dispatcher.h"

/* probably implemented as a heap */
dispatcher_t* new_dispatcher(void)
{
	dispatcher_t* ret = shm_malloc(sizeof (*ret));

	if (ret == NULL)
	{
		LM_ERR("Allocating dispatcher\n");
		return NULL;
	}

	ret->size = 0;
	ret->lock = lock_alloc();

	if (ret->lock == NULL)
	{
		LM_ERR("Allocating lock\n");
		return NULL;
	}

	lock_init(ret->lock);

	sem_init(&ret->sem, 0, 0);

	return ret;
}

void destroy_dispatcher(dispatcher_t* disp)
{
	lock_destroy(disp->lock);
	lock_dealloc(disp->lock);
	sem_destroy(&disp->sem);
	shm_free(disp);
}

/* will be called by the reactor thread and timer thread*/
void put_task(dispatcher_t* d, heap_node_t n)
{

	heap_node_t *nodes, tmp;

	nodes = d->nodes;
	int cur, parent;

	lock_get(d->lock);

	cur = d->size;

	nodes[cur] = n;
	d->size++;

	while (cur > 0)
	{
		parent = (cur - 1) / 2;

		if (nodes[cur].priority > nodes[parent].priority)
		{
			tmp = nodes[parent];
			nodes[parent] = nodes[cur];
			nodes[cur] = tmp;
		} else
			break;

		cur = parent;
	}


	lock_release(d->lock);


	sem_post(&d->sem);

}

/* will be called by the worker threads */
void get_task(dispatcher_t* d, heap_node_t * n)
{

	int cur, size, maxi, maxval, son1, son2;
	heap_node_t *nodes, tmp;

	nodes = d->nodes;

	sem_wait(&d->sem);

	lock_get(d->lock);

	*n = nodes[0];

	nodes[0] = nodes[d->size - 1];
	d->size--;

	cur = 0;
	size = d->size;

	while (cur * 2 + 1 < size)
	{
		maxi = cur;
		maxval = nodes[cur].priority;

		son1 = cur * 2 + 1;
		son2 = cur * 2 + 2;

		if (nodes[son1].priority > maxval)
		{
			maxi = son1;
			maxval = nodes[son1].priority;
		}

		if (son2 < size && nodes[son2].priority > maxval)
		{
			maxi = son2;
			maxval = nodes[son2].priority;
		}

		if (maxi != cur)
		{

			tmp = d->nodes[cur];
			d->nodes[cur] = d->nodes[maxi];
			d->nodes[maxi] = tmp;

			cur = maxi;
		} else
			break;
	}

	lock_release(d->lock);
}


void inline put_task_simple(dispatcher_t* d, int priority,
		fd_callback *cb, void* cb_param)
{
	heap_node_t task;

	task.last_reactor = NULL;
	task.fd = -1;
	
	task.flags = 0;
	task.priority = priority;
	task.cb = (fd_callback*) cb;
	task.cb_param = (void*) cb_param;

	put_task(d, task);
}

