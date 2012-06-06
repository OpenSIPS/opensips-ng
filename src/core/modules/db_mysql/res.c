/* 
 * $Id: res.c 5901 2009-07-21 07:45:05Z bogdan_iancu $ 
 *
 * MySQL module result related functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2007-2008 1&1 Internet AG
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


#include <string.h>
#include <mysql/mysql.h>
#include "../../db/db_res.h"
#include "../../mem/mem.h"
#include "../../log.h"
#include "row.h"
#include "my_con.h"
#include "res.h"


/**
 * Get and convert columns from a result
 */
int db_mysql_get_columns(const db_con_t* _h, db_res_t* _r)
{
	int col;
	MYSQL_FIELD* fields;

	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (CON_HAS_PS(_h)) {
		RES_COL_N(_r) = CON_MYSQL_PS(_h)->cols_out;
	} else {
		RES_COL_N(_r) = mysql_field_count(CON_CONNECTION(_h));
	}
	if (!RES_COL_N(_r)) {
		LM_ERR("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}
	
	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("could not allocate columns\n");
		return -3;
	}

	fields = mysql_fetch_fields(CON_RESULT(_h));
	for(col = 0; col < RES_COL_N(_r); col++) {
		/* The pointer that is here returned is part of the result structure */
		RES_NAMES(_r)[col]->s = fields[col].name;
		RES_NAMES(_r)[col]->len = strlen(fields[col].name);

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);

		switch(fields[col].type) {
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			case MYSQL_TYPE_INT24:
			case MYSQL_TYPE_LONGLONG:
			case MYSQL_TYPE_DECIMAL:
			#if MYSQL_VERSION_ID > 49999
			case MYSQL_TYPE_NEWDECIMAL:
			#endif
			case MYSQL_TYPE_TIMESTAMP:
				LM_DBG("use DB_INT result type\n");
				RES_TYPES(_r)[col] = DB_INT;
				break;

			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
				LM_DBG("use DB_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB_DOUBLE;
				break;

			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_DATE:
				LM_DBG("use DB_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB_DATETIME;
				break;

			case MYSQL_TYPE_BLOB:
				LM_DBG("use DB_BLOB result type\n");
				RES_TYPES(_r)[col] = DB_BLOB;
				break;

			case FIELD_TYPE_SET:
				LM_DBG("use DB_BITMAP result type\n");
				RES_TYPES(_r)[col] = DB_BITMAP;
				break;

			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_VAR_STRING:
				LM_DBG("use DB_STRING result type\n");
				RES_TYPES(_r)[col] = DB_STRING;
				break;

			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use DB_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, fields[col].type);
				RES_TYPES(_r)[col] = DB_STRING;
				break;
		}
	}
	return 0;
}

/**
 * Fill the structure with data from database
 */
int db_mysql_fill_result(const db_con_t* _h, db_res_t* _r)
{
	if ((!_h) || (!_r)) {
		LM_ERR("invalid parameter\n");
		return -1;
	}

	if (db_mysql_get_columns(_h, _r) < 0) {
		LM_ERR("error while getting column names\n");
		return -2;
	}

	if (CON_HAS_PS(_h)) {
		RES_NUM_ROWS(_r) = mysql_stmt_num_rows(CON_PS_STMT(_h));
	} else {
		RES_NUM_ROWS(_r) = mysql_num_rows(CON_RESULT(_h));
	}

	return 0;
}

