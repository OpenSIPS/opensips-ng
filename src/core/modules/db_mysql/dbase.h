/*
 * $Id: dbase.h 5901 2009-07-21 07:45:05Z bogdan_iancu $
 *
 * MySQL module core functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 */


#ifndef DBASE_H
#define DBASE_H


#include "../../db/db_con.h"
#include "../../db/db_res.h"
#include "../../db/db_key.h"
#include "../../db/db_op.h"
#include "../../db/db_val.h"
#include "../../db/db_row.h"
#include "../../str.h"
#include "my_con.h"

/*
 * Initialize database connection
 */
void* db_mysql_init(db_id_t* id);


/*
 * Close a database connection
 */
void db_mysql_close(db_con_t* _h);


/*
 * Free all memory allocated by get_result
 */
void db_mysql_free_result(void *data);


/*
 * Do a query
 */
int db_mysql_query( db_con_t* _h,  db_key_t* _k,  db_op_t* _op,
	      db_val_t* _v,  db_key_t* _c,  int _n,  int _nc,
	      db_key_t _o, db_res_t** _r);


/*
 * fetch rows from a result
 */
int db_mysql_fetch_result( db_con_t* _h, db_res_t** _r,  int nrows);


/*
 * Raw SQL query
 */
int db_mysql_raw_query( db_con_t* _h,  str* _s, db_res_t** _r);


/*
 * Insert a row into table
 */
int db_mysql_insert( db_con_t* _h,  db_key_t* _k,  db_val_t* _v,  int _n);


/*
 * Delete a row from table
 */
int db_mysql_delete( db_con_t* _h,  db_key_t* _k,
	db_op_t* _o,  db_val_t* _v,  int _n);


/*
 * Update a row in table
 */
int db_mysql_update( db_con_t* _h,  db_key_t* _k,  db_op_t* _o,
	 db_val_t* _v,  db_key_t* _uk,  db_val_t* _uv,  int _n,
	 int _un);


/*
 * Returns the last inserted ID
 */
int db_mysql_last_inserted_id( db_con_t* _h);

/*
 * Insert a row into table, update on duplicate key
 */
int db_insert_update_func( db_con_t* _h,  db_key_t* _k,  db_val_t* _v,
	 int _n);


/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_mysql_use_table(db_con_t* _h,  str* _t);


/*
 *	Free all allocated prep_stmt structures
 */
void db_mysql_free_stmt_list(struct prep_stmt *head);

/* get next row from the specified connection 
 */
int db_mysql_fetch_next_row(db_res_t *_res,db_row_t *_r);

int db_mysql_socket(db_con_t *h);

int db_mysql_resume(db_con_t *h,db_res_t **r);
#endif /* DBASE_H */
