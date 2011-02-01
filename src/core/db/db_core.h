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

#ifndef _CORE_DB
#define _CORE_DB

#include "db_key.h"
#include "db_op.h"
#include "db_val.h"
#include "../locking/locking.h"
#include "../str.h"
#include "db_id.h"
#include "db_con.h"
#include "db_res.h"
#include "../threading.h"
#include "db_to_module.h"

struct _db_pool;

enum
{
	OP_QUERY,
	OP_RAW,

	OP_INSERT,
	OP_DELETE,
	OP_UPDATE,
	OP_INSERT_UPDATE
};

/*
 * A db_item is a wrapper for a connection.
 *
 */
typedef struct _db_item
{
	db_con_t* connection;	/* actual connection to be passed to the module*/
	struct _db_pool * pool;	/* the pool to which this connection belongs to */
	struct _db_item * next;

}db_item_t;

/*
 * A db_query contains all the information to issue the query,
 * and return the result via the callback
 */

typedef struct _db_query
{
	/* information describing the query */
	int type;

	str * table;
	db_key_t* k;
	db_op_t* op;
	db_val_t* v;
	int n;
	db_key_t* c;
	int nc;
	db_key_t* uk;
	db_val_t* uv;
	int nu;
	db_key_t o;
	str* raw;
	int * ps_idx;

	/* callback information */
	void * arg;
	void * func;

	/* query run-time inforamtion */
	db_item_t* item;		/* connection on which this query was issued */
	db_res_t* res;			/* query result structure */
	int ret;				/* query return value */

	/* pointer to next query in the list */
	struct _db_query * next;

}db_query_t;

/*
 * A db_pool contains a list of connections to the same database,
 * and a list of queries issued to the given database
 */
typedef struct _db_pool
{
	db_id_t * id;			/* the id of the connections in the pool */
	struct db_func * funcs;	/* the functions provided by the module */

	gen_lock_t * lock;		/* lock to protect list operations */
	db_item_t * items;		/* list of free connections*/
	db_query_t * queries;	/* list of issued queries */

	int ps_count;			/* number of prepared statements in current pool */

	struct _db_pool * next;	/* pointer to next pool in list */

	
}db_pool_t;

/* Submit a query to be sent on a given free connection */
void db_submit_active_query(	db_item_t * item, db_query_t * query);
/* Submit a query to a given pool, if any connection is free it is sent,
 * otherwise it is queud */
void db_submit_query( db_pool_t * pool, db_query_t * query);
/* Free a connection, if there are any queued queries one will be sent,
 * otherwise the connection will be placed in the pool */
void db_submit_item( db_item_t * item);

/* start a number of db threads */
void db_threads(int n);

/* register a module with a given name */
void db_set_module( char * name, struct db_func* funcs);
/* find a module by it's name */
struct db_func* db_get_module(char * name);

#endif
