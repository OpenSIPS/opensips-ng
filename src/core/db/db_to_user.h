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


#ifndef USER_TO_DB_H
#define USER_TO_DB_H

#include "db_to_module.h"
#include "db_core.h"
#include "db_key.h"
#include "db_op.h"
#include "db_val.h"
#include "db_con.h"
#include "db_res.h"
#include "db_cap.h"
#include "db_con.h"
#include "db_row.h"
#include "db_ps.h"
#include "../locking/locking.h"

//TODO restore comments
//TODO see how to workaround freeing memory

typedef void (* db_res_answer_f) (void * arg, db_res_t * res, int ret);
typedef void (* db_op_answer_f) (void * arg, int ret);


/* initialize the database part of the core,
 * must be called only once */
int db_core_init();

void db_core_destroy(void);

int db_start_threads(int count);

/* Get a connection pool */
db_pool_t* db_init(str * sqlurl, int capabilities);

void db_close(db_pool_t* _h);

void db_free_result(db_pool_t* _h, db_res_t* _r);

/* functions to handle the results */
int db_fetch_result(db_item_t* _h, db_res_t** _r, int _n);

void db_finalize_fetch(db_res_t* _i);

int db_fetch_row(db_pool_t* h, db_res_t* r,db_row_t* row);

void db_select(db_pool_t* _h, str * table,
		db_key_t* _k, db_op_t* _op,
		db_val_t* _v, db_key_t* _c, int _n, int _nc,
		db_key_t _o, int * ps_idx,
		db_res_answer_f func, void * arg);

void db_raw_query(db_pool_t* _h, str * table,
		str* _s,
		db_res_answer_f func, void * arg);

void db_insert(db_pool_t* _h, str * table,
		db_key_t* _k, db_val_t* _v,
		int _n, int * ps_idx,
		db_op_answer_f func, void * arg);

void db_delete(db_pool_t* _h, str * table,
		db_key_t* _k, db_op_t* _o,
		db_val_t* _v, int _n, int * ps_idx,
		db_op_answer_f func, void * arg);

void db_update(db_pool_t* _h, str * table,
		db_key_t* _k, db_op_t* _o,
		db_val_t* _v, db_key_t* _uk, db_val_t* _uv,
		int _n, int _un, int * ps_idx,
		db_op_answer_f func, void * arg);

void db_insert_update(db_pool_t* _h, str * table,
		db_key_t* _k, db_val_t* _v, int _n,
		db_op_answer_f func, void * arg);

int db_last_inserted_id(db_pool_t* _h, str * table);

#endif /* USER_TO_DB_H */
