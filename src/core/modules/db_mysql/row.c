/* 
 * $Id: row.c 6531 2010-01-25 16:05:26Z bogdan_iancu $ 
 *
 * MySQL module row related functions
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


#include "../../log.h"
#include "../../mem/mem.h"
#include "../../db/db_row.h"
#include "../../db/db_ut.h"
#include "my_con.h"
#include "val.h"
#include "row.h"


/**
 * Convert a row from result into db API representation
 */
int db_mysql_convert_row(const db_con_t* _h,db_res_t *_res,db_row_t* _r)
{
	unsigned long* lengths;
	int i;

	if ((!_h) || (!_r)) 
	{
		LM_ERR("invalid parameter value\n");
		return -1;
	}

	if (CON_HAS_PS(_h)) 
	{
		for(i=0; i < CON_MYSQL_PS(_h)->cols_out; i++) 
		{
			if (db_mysql_str2val(RES_TYPES(_res)[i], &(ROW_VALUES(_r)[i]),
			CON_PS_OUTCOL(_h, i).null?NULL:CON_PS_OUTCOL(_h, i).buf,
			CON_PS_OUTCOL(_h,i).len) < 0) 
			{
				LM_ERR("failed to convert value from stmt\n");
				db_free_row_vals(_r);
				return -1;
			}
		}
	} 
	else 
	{
		lengths = mysql_fetch_lengths(CON_RESULT(_h));
		for(i = 0; i < RES_COL_N(_res); i++) {
			if (db_mysql_str2val(RES_TYPES(_res)[i], &(ROW_VALUES(_r)[i]),
			((MYSQL_ROW)CON_ROW(_h))[i], lengths[i]) < 0) {
				LM_ERR("failed to convert value\n");
				db_free_row_vals(_r);
				return -1;
			}
		}
	}

	return 0;
}