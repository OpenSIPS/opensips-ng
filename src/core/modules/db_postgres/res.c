/*
 * $Id: res.c 6451 2009-12-21 17:18:32Z bogdan_iancu $
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
 * Copyright (C) 2006 Norman Brandinger
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
 * 2006-07-26 added BPCHAROID as a valid type for DB_STRING conversions
 *            this removes the "unknown type 1042" log messages (norm)
 *
 * 2006-10-27 Added fetch support (norm)
 *            Removed dependency on aug_* memory routines (norm)
 *            Added connection pooling support (norm)
 *            Standardized API routines to pg_* names (norm)
 *
 */

#include <stdlib.h>
#include <string.h>
#include "../../db/db_id.h"
#include "../../db/db_res.h"
#include "../../db/db_con.h"
#include "../../log.h"
#include "../../mem/mem.h"
#include "res.h"
#include "val.h"
#include "pg_con.h"
#include "pg_type.h"



/**
 * Fill the result structure with data from the query
 */
int db_postgres_convert_result(const db_con_t* _h, db_res_t* _r)
{
	if (!_h || !_r)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (db_postgres_get_columns(_h, _r) < 0) {
		LM_ERR("failed to get column names\n");
		return -2;
	}

	return 0;
}

/**
 * Get and convert columns from a result set
 */
int db_postgres_get_columns(const db_con_t* _h, db_res_t* _r)
{
	int col, datatype;

	if (!_h || !_r)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* Get the number of rows (tuples) in the query result. */
	RES_NUM_ROWS(_r) = PQntuples(_r->data);

	/* Get the number of columns (fields) in each row of the query result. */
	RES_COL_N(_r) = PQnfields(_r->data);

	if (!RES_COL_N(_r)) {
		LM_DBG("no columns returned from the query\n");
		return -2;
	} else {
		LM_DBG("%d columns returned from the query\n", RES_COL_N(_r));
	}

	if (db_allocate_columns(_r, RES_COL_N(_r)) != 0) {
		LM_ERR("could not allocate columns\n");
		return -3;
	}

	/* For each column both the name and the OID number of the 
	 * data type are saved. */
	for(col = 0; col < RES_COL_N(_r); col++) {
		/* The pointer that is here returned is part of the result structure.*/
		RES_NAMES(_r)[col]->s = PQfname(_r->data, col);
		RES_NAMES(_r)[col]->len = strlen(PQfname(_r->data, col));

		LM_DBG("RES_NAMES(%p)[%d]=[%.*s]\n", RES_NAMES(_r)[col], col,
				RES_NAMES(_r)[col]->len, RES_NAMES(_r)[col]->s);

		/* get the datatype of the column */
		switch(datatype = PQftype(_r->data,col))
		{
			case INT2OID:
			case INT4OID:
			case INT8OID:
				LM_DBG("use DB_INT result type\n");
				RES_TYPES(_r)[col] = DB_INT;
			break;

			case FLOAT4OID:
			case FLOAT8OID:
			case NUMERICOID:
				LM_DBG("use DB_DOUBLE result type\n");
				RES_TYPES(_r)[col] = DB_DOUBLE;
			break;

			case DATEOID:
			case TIMESTAMPOID:
			case TIMESTAMPTZOID:
				LM_DBG("use DB_DATETIME result type\n");
				RES_TYPES(_r)[col] = DB_DATETIME;
			break;

			case BOOLOID:
			case CHAROID:
			case VARCHAROID:
			case BPCHAROID:
				LM_DBG("use DB_STRING result type\n");
				RES_TYPES(_r)[col] = DB_STRING;
			break;

			case TEXTOID:
			case BYTEAOID:
				LM_DBG("use DB_BLOB result type\n");
				RES_TYPES(_r)[col] = DB_BLOB;
			break;

			case BITOID:
			case VARBITOID:
				LM_DBG("use DB_BITMAP result type\n");
				RES_TYPES(_r)[col] = DB_BITMAP;
			break;
				
			default:
				LM_WARN("unhandled data type column (%.*s) type id (%d), "
						"use DB_STRING as default\n", RES_NAMES(_r)[col]->len,
						RES_NAMES(_r)[col]->s, datatype);
				RES_TYPES(_r)[col] = DB_STRING;
			break;
		}
	}
	return 0;
}

/**
 * Convert a row from the result query into db API representation
 */
int db_postgres_convert_row(const db_con_t* _h, db_res_t* _r, db_row_t* _row,
		char **row_buf)
{
	int col, len;

	if (!_h || !_r || !_row)  {
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	/* Save the number of columns in the ROW structure */
	ROW_N(_row) = RES_COL_N(_r);

	/* For each column in the row */
	for(col = 0; col < ROW_N(_row); col++) {
		/* compute the len of the value */
		if ( row_buf[col]==NULL || row_buf[col][0]=='\0')
			len = 0;
		else
			len =  strlen(row_buf[col]);

		/* Convert the string representation into the value representation */
		if (db_postgres_str2val(RES_TYPES(_r)[col], &(ROW_VALUES(_row)[col]),
		row_buf[col], len) < 0) {
			LM_ERR("failed to convert value\n");
			LM_DBG("free row at %pn", _row);
			db_free_row_vals(_row);
			return -3;
		}
	}
	return 0;
}
