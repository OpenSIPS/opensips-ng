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



#include <string.h>

#include "db_core.h"
#include "db_to_user.h"
#include "../map.h"
#include "../locking/locking.h"
#include "db_id.h"
#include "db_globals.h"
#include "db_ps.h"


db_pool_t * pool_list = NULL;
gen_lock_t * db_pools_lock = NULL;
int db_thread_count = 4;
int db_connection_count = 8;

int db_core_init(void)
{
	/* variables needed to manage the list of connection pools*/
	db_pools_lock = lock_alloc();
	
	if( db_pools_lock == NULL )
		goto error;

	lock_init(db_pools_lock);

	/* variables needed to manage the list of queries that are pending*/
	waiting = NULL;
	sem_init(&w_sem, 0, 0);
	w_lock = lock_alloc();

	if( w_lock == NULL )
		goto error;

	lock_init(w_lock);

	/* variables needed to manage the prepared statements index */
	ps_count = 0;
	ps_lock = lock_alloc();

	if( ps_lock == NULL )
		goto error;

	lock_init(ps_lock);

	/* variables needed for registering modules */
	modules_lock = lock_alloc();

	if( modules_lock == NULL )
		goto error;

	lock_init(modules_lock);

	return 0;

error:
	return -1;
}


void db_core_destroy(void)
{
	if (db_pools_lock) {
		lock_destroy(db_pools_lock);
		lock_dealloc(db_pools_lock);
	}
	if (w_lock) {
		lock_destroy(w_lock);
		lock_dealloc(w_lock);
	}
	if (ps_lock) {
		lock_destroy(ps_lock);
		lock_dealloc(ps_lock);
	}
	if (modules_lock) {
		lock_destroy(modules_lock);
		lock_dealloc(modules_lock);
	}
}


int db_start_threads(int count)
{
	db_threads(count);
	return 0;
}

db_con_t* db_do_init( db_id_t * id, db_func_t * funcs)
{
	db_con_t* res;

	/* this is the root memory for this database connection. */
	res = (db_con_t*)shm_malloc(sizeof(*res));
	if (!res) {
		LM_ERR("no private memory left\n");
		return 0;
	}
	memset(res, 0, sizeof(*res));

	res->tail = funcs->init(id);

	return res;

 }

db_pool_t* db_init(str * sqlurl, int capabilities)
{

	db_pool_t *p;
	db_id_t * id = new_db_id(sqlurl);
	db_func_t * funcs;
	int i;
	db_item_t * cur;

	lock_get(db_pools_lock);

	for (p = pool_list; p; p = p->next)
	{
		if (cmp_db_id(p->id, id))
			break;
	}

	if (p == NULL)
	{
		p = shm_malloc(sizeof (*p));

		if( p == NULL)
		{
			LM_ERR("Unable to allocate memory\n");
			goto end;
		}
		
		memset(p, 0, sizeof (*p));

		p->lock = lock_alloc();
		
		if( p->lock == NULL )
		{
			LM_ERR("Unable to allocate memory\n");
			goto end;
		}

		lock_init(p->lock);

		p->id = id;
		p->next = pool_list;
		pool_list = p;

		funcs = db_get_module(id->scheme);

		if( funcs == NULL)
		{
			LM_ERR("Unable to find module for %s\n",id->scheme);
			goto end;
		}

		if( (funcs->cap & capabilities) != capabilities )
		{
			LM_ERR("Module %s does not support all the necessary capabilities"
					"wanted %d have %d\n",id->scheme, capabilities, funcs->cap);
			goto end;
		}

		p->funcs = funcs;

		/* starting several connections in this pool */
		for (i = 0; i < db_connection_count; i++)
		{
			cur = shm_malloc(sizeof (*cur));

			if( cur == NULL )
			{
				LM_ERR("Unable to allocate memory\n");
				goto end;
			}
			
			cur->connection = db_do_init(id, funcs);

			if( cur->connection == NULL)
			{
				LM_ERR("Unable to initialize connection number %d\n",i);
				goto end;
			}
			cur->pool = p;
			cur->next = p->items;
			p->items = cur;
		}

	}

end:
	lock_release(db_pools_lock);
	return p;
}

void db_close(db_pool_t* _h)
{
	//TODO
}

void db_select(db_pool_t* _h, str * table,
						 db_key_t* _k, db_op_t* _op,
						 db_val_t* _v, db_key_t* _c, int _n, int _nc,
						 db_key_t _o, int * ps_idx,
						 db_res_answer_f func, void * arg)
{
	db_query_t * q = shm_malloc(sizeof (*q));

	if(q)
	{
		q->type = OP_QUERY;
		q->table = table;
		q->k = _k;
		q->op = _op;
		q->v = _v;
		q->c = _c;
		q->n = _n;
		q->nc = _nc;
		q->o = _o;
		q->ps_idx = ps_idx;

		q->func = func;
		q->arg = arg;

		db_submit_query(_h, q);
	}
	else
	{
		LM_ERR("Unable to allocate memory\n");
		func(arg, NULL, -1);
	}
}

void db_raw_query(db_pool_t* _h, str * table,
							 str* _s,
							 db_res_answer_f func, void * arg)
{
	db_query_t * q = shm_malloc(sizeof (*q));

	if(q)
	{
		q->type = OP_RAW;
		q->table = table;
		q->raw = _s;

		q->func = func;
		q->arg = arg;
		q->ps_idx = NULL;

		db_submit_query(_h, q);
	}
	else
	{
		LM_ERR("Unable to allocate memory\n");
		func(arg, NULL, -1);
	}
}

void db_insert(db_pool_t* _h, str * table,
			   db_key_t* _k, db_val_t* _v,
			   int _n, int * ps_idx,
			   db_op_answer_f func, void * arg)
{
	db_query_t * q = shm_malloc(sizeof (*q));

	if(q)
	{
		q->type = OP_INSERT;
		q->table = table;
		q->k = _k;
		q->v = _v;
		q->n = _n;
		q->ps_idx = ps_idx;

		q->func = func;
		q->arg = arg;

		db_submit_query(_h, q);
	}
	else
	{
		LM_ERR("Unable to allocate memory\n");
		func(arg, -1);
	}
}

void db_delete(db_pool_t* _h, str * table,
			   db_key_t* _k, db_op_t* _op,
			   db_val_t* _v, int _n, int * ps_idx,
			   db_op_answer_f func, void * arg)
{
	db_query_t * q = shm_malloc(sizeof (*q));

	if( q )
	{
		q->type = OP_DELETE;
		q->table = table;
		q->k = _k;
		q->v = _v;
		q->op = _op;
		q->n = _n;
		q->ps_idx = ps_idx;

		q->func = func;
		q->arg = arg;

		db_submit_query(_h, q);
	}
	else
	{
		LM_ERR("Unable to allocate memory\n");
		func(arg, -1);
	}
}

void db_update(db_pool_t* _h, str * table,
			   db_key_t* _k, db_op_t* _op,
			   db_val_t* _v, db_key_t* _uk, db_val_t* _uv,
			   int _n, int _un, int * ps_idx,
			   db_op_answer_f func, void * arg)
{
	db_query_t * q = shm_malloc(sizeof (*q));

	if(q)
	{
		q->type = OP_UPDATE;
		q->table = table;
		q->k = _k;
		q->v = _v;
		q->op = _op;
		q->uk = _uk;
		q->uv = _uv;
		q->n = _n;
		q->nu = _un;
		q->ps_idx = ps_idx;

		q->func = func;
		q->arg = arg;

		db_submit_query(_h, q);
	}
	else
	{
		LM_ERR("Unable to allocate memory\n");
		func(arg, -1);
	}

}

void db_insert_update(db_pool_t* _h, str * table,
					  db_key_t* _k, db_val_t* _v, int _n,
					  db_op_answer_f func, void * arg)
{
	db_query_t * q = shm_malloc(sizeof (*q));

	if(q)
	{
		q->type = OP_INSERT_UPDATE;
		q->table = table;
		q->k = _k;
		q->v = _v;
		q->n = _n;
		q->func = func;
		q->arg = arg;

		db_submit_query(_h, q);
	}
	else
	{
		LM_ERR("Unable to allocate memory\n");
		func(arg, -1);
	}
}

//int db_last_inserted_id (db_pool_t* _h,   str * table);

void db_finalize_fetch(db_res_t* _i)
{
	if (_i)
	{
		CON_RESET_CURR_PS(_i->it->connection);
		db_submit_item(_i->it);
	}
}

int db_fetch_row(db_pool_t* h, db_res_t* r,db_row_t* row)
{
	return h->funcs->fetch_next_row(r, row);
}

void db_free_result(db_pool_t* _h, db_res_t* _r)
{
	if( _r )
	{
		if (_r->data)
			_h->funcs->free_result(_r->data);
		db_mem_free_result(_r);
	}
	else
	{
		LM_ERR("Trying to free NULL result\n");
	}

}
