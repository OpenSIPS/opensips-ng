/*
 * $Id: dbase.h 5898 2009-07-20 16:41:39Z bogdan_iancu $
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2008 1&1 Internet AG
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
 * ---
 *
 * History
 * -------
 * 2003-04-06 initial code written (Greg Fausak/Andy Fullford)
 *
 */


#ifndef DBASE_H
#define DBASE_H

#include "../../db/db_con.h"
#include "../../db/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"


/**
 * Initialize database connection
 */
void* db_postgres_init(struct db_id* id);

/**
 * Close a database connection
 */
void db_postgres_close(db_con_t* _h);

/**
 * Return result of previous query
 */
int db_postgres_store_result(db_con_t* _h, db_res_t** _r);


/**
 * Free all memory allocated by get_result
 */
void db_postgres_free_result(void * data);


/**
 * Do a query
 */
int db_postgres_query(db_con_t* _h, db_key_t* _k, db_op_t* _op,
		db_val_t* _v, db_key_t* _c, int _n, int _nc,
		db_key_t _o, db_res_t** _r);

/**
 * Raw SQL query
 */
int db_postgres_raw_query(db_con_t* _h, str* _s, db_res_t** _r);

/**
 * Fetch next row from the result 
 */
int db_postgres_fetch_row(db_res_t *_r,db_row_t* _rw);
/**
 * Insert a row into table
 */
int db_postgres_insert(db_con_t* _h, db_key_t* _k, db_val_t* _v,
		int _n);


/**
 * Delete a row from table
 */
int db_postgres_delete(db_con_t* _h, db_key_t* _k, db_op_t* _o,
		db_val_t* _v, int _n);


/**
 * Update a row in table
 */
int db_postgres_update(db_con_t* _h, db_key_t* _k, db_op_t* _o,
		db_val_t* _v, db_key_t* _uk, db_val_t* _uv, int _n,
		int _un);

/**
 * fetch rows from a result
 */
int db_postgres_fetch_result(db_con_t* _h, db_res_t** _r, int nrows);


/**
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_postgres_use_table(db_con_t* _h, str* _t);


int db_postgres_socket (db_con_t* h);

int db_postgres_resume (  db_con_t* _h, db_res_t ** r);

#endif /* DBASE_H */
