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
 *  2010-06-xx  created (adragus)
 */

#include "db_core.h"
#include <semaphore.h>
#include "../threading.h"
#include "../dispatcher/dispatcher.h"
#include "../globals.h"
#include "db_to_user.h"
#include "../reactor/reactor.h"

typedef struct _db_module
{
	char * name;
	db_func_t * funcs;
	struct _db_module * next;

} db_module_t;


db_module_t * modules = NULL;
gen_lock_t * modules_lock = NULL;


db_query_t * waiting;
sem_t w_sem;
gen_lock_t * w_lock = NULL;


/* method that sends a query,
 * if the module is non-blocking so is this function
 */
int send_query(db_query_t * q)
{
	db_func_t * funcs;
	db_con_t * conn;
	db_res_t * res = NULL;
	int ret = 0;

	funcs = q->item->pool->funcs;
	conn = q->item->connection;

	funcs->use_table(conn, q->table);
	conn->ps_idx = q->ps_idx;

	switch (q->type)
	{
	case OP_QUERY:
		ret = funcs->select(conn, q->k, q->op, q->v, q->c, q->n, q->nc, q->o, &res);
		break;
	case OP_RAW:
		ret = funcs->raw_query(conn, q->raw, &res);
		break;
	case OP_INSERT:
		ret = funcs->insert(conn, q->k, q->v, q->n);
		break;

	case OP_DELETE:
		ret = funcs->delete(conn, q->k, q->op, q->v, q->n);
		break;

	case OP_UPDATE:
		ret = funcs->update(conn, q->k, q->op, q->v, q->uk, q->uv, q->n,
							q->nu);
		break;

	case OP_INSERT_UPDATE:
		ret = funcs->insert_update(conn, q->k, q->v, q->n);
		break;
	}

	q->ret = ret;
	q->res = res;
	return ret;
}

/* function that receives a query result and calls the necessary callback,
 * it can be added as a callback in the dispatcher
 */
int unpack_result(void *param)
{
	LM_DBG("unpacking result\n");

	db_query_t * q = (db_query_t *) param;
	db_res_answer_f f;
	db_op_answer_f h;

	switch (q->type)
	{
	case OP_QUERY:
	case OP_RAW:
		if (q->res)
			q->res->it = q->item;
		else
			db_submit_item(q->item);
		f = (db_res_answer_f) q->func;
		f(q->arg, q->res, q->ret);
		break;

	case OP_INSERT:
	case OP_DELETE:
	case OP_UPDATE:
	case OP_INSERT_UPDATE:
		h = (db_op_answer_f) q->func;
		h(q->arg, q->ret);
		break;

	}

	shm_free(q);

	return 0;
}

/* method that tries to read events after a query was issued */
int continue_query(void *param)
{
	LM_DBG("continue query called \n");
	db_query_t * q = (db_query_t *) param;
	db_item_t * item = q->item;
	db_func_t * funcs = item->pool->funcs;
	int ret;
	int sock;

	if (q->type == OP_QUERY || q->type == OP_RAW)
		ret = funcs->resume(item->connection, &q->res);
	else
		ret = funcs->resume(item->connection, NULL);

	if (ret <= 0)
		goto end;

	sock = funcs->socket(item->connection);
	submit_task(reactor_in, continue_query, q, TASK_PRIO_READ_IO, sock, 0);

	return 0;

end:

	if (q->type != OP_RAW && q->type != OP_QUERY)
	{
		db_submit_item(item);
	}

	return unpack_result(q);
}

/*
 * Method that is called to pair-up a db_item with a db_query,
 * in order to submit the query via the item.
 *
 */
void db_submit_active_query(db_item_t * item, db_query_t * q)
{
	db_func_t * funcs = item->pool->funcs;
	int sock;
	int ret;

	q->item = item;
	/* if the module is in blocking mode */
	if (funcs->socket == NULL)
	{
		LM_DBG("module in blocking mode\n");
blocking:
		lock_get(w_lock);

		q->next = waiting;
		waiting = q;

		lock_release(w_lock);

		sem_post(&w_sem);
	} else
	{
		//TODO error checking
		/* MYSQL module doesn't support async prep stmts so we do blocking query */
		if ((q->ps_idx) && (funcs->cap | DB_CAP_SYNC_PREP_STMT))
			goto blocking;

		ret = send_query(q);
		LM_DBG("module in non-blocking mode. ret = %d\n",ret);
		if (ret >= 0)
		{
			sock = funcs->socket(item->connection);
			submit_task(reactor_in, continue_query, q, TASK_PRIO_READ_IO, sock, 0);
		}
	}

}

/* send a query to the core */
void db_submit_query(db_pool_t * pool, db_query_t * query)
{
	db_item_t * item;

	lock_get(pool->lock);

	/* if there is a free item pair them up */
	if (pool->items != NULL)
	{
		item = pool->items;
		pool->items = item->next;
		item->next = NULL;
	
		db_submit_active_query(item, query);
	} else
	{
		query->next = pool->queries;
		pool->queries = query;
	}

	lock_release(pool->lock);
}

/* send an item to the core */
void db_submit_item(db_item_t * item)
{
	db_query_t * query;
	db_pool_t * pool = item->pool;

	lock_get(pool->lock);

	/* if there are any pending queries in this pool, pair it with the item */
	if (pool->queries != NULL)
	{
		query = pool->queries;
		pool->queries = query->next;
		query->next = NULL;

		db_submit_active_query(item, query);
	} else
	{
		item->next = pool->items;
		pool->items = item;
	}

	lock_release(pool->lock);

}

/* function called by a thread worker */
void * work(void * arg)
{
	db_query_t * q;
	db_item_t * item;
	int ret;

	while (1)
	{
		/* wait until there is a query to be issued and a free connection */
		sem_wait(&w_sem);

		lock_get(w_lock);

		q = waiting;
		waiting = q->next;
		item = q->item;

		lock_release(w_lock);

		/* send the query */
		ret = send_query(q);

		/* put the result through the dispatcher */
		put_task_simple(reactor_in->disp, TASK_PRIO_RESUME_EXEC, unpack_result, q);

		/* if the query was not a SELECT, resubmit the connection into the pool*/
		if (q->type != OP_RAW && q->type != OP_QUERY)
		{
			db_submit_item(item);
		}

	}

	return NULL;
}

void db_threads(int n)
{
	//TODO check out how to control number of workers
	int i = 0;

	for (i = 0; i < n; i++)
	{
		pt_create_thread("db_worker", work, NULL);
	}

}

void db_set_module(char * name, db_func_t* funcs)
{
	db_module_t * m = shm_malloc(sizeof (*m));

	lock_get(modules_lock);

	m->name = name;
	m->funcs = funcs;
	m->next = modules;

	modules = m;

	lock_release(modules_lock);
}

db_func_t * db_get_module(char * scheme)
{
	db_module_t * p;
	db_func_t * answer = NULL;

	lock_get(modules_lock);

	for (p = modules; p; p = p->next)
	{
		if (strcasecmp(p->name, scheme) == 0)
		{
			answer = p->funcs;
			break;
		}
	}

	lock_release(modules_lock);
	
	return answer;
}
