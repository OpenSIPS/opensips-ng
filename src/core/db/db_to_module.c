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



#include "../log.h"
#include "../mem/mem.h"
#include "../utils.h"
#include "db_cap.h"
#include "db_to_module.h"
#include <string.h>
#include "../locking/locking.h"

//TODO check version/ get version


#define VERSION_TABLE "version"
#define TABLENAME_COLUMN "tablename"
#define VERSION_COLUMN "version"

char *db_version_table = VERSION_TABLE;

gen_lock_t * ps_lock = NULL;
int ps_count;


int db_check_api(db_func_t* dbf, char *mname)
{
	if(dbf==NULL)
		return -1;

	/* All modules must export db_use_table */
	if (dbf->use_table == 0) {
		LM_ERR("module %s does not export db_use_table function\n", mname);
		goto error;
	}

	/* All modules must export db_init */
	if (dbf->init == 0) {
		LM_ERR("module %s does not export db_init function\n", mname);
		goto error;
	}

	/* All modules must export db_close */
	if (dbf->close == 0) {
		LM_ERR("module %s does not export db_close function\n", mname);
		goto error;
	}

	if (dbf->select) {
		dbf->cap |= DB_CAP_QUERY;
	}

	if (dbf->raw_query) {
		dbf->cap |= DB_CAP_RAW_QUERY;
	}

	/* Free result must be exported if DB_CAP_QUERY or
	 * DB_CAP_RAW_QUERY is set */
	if ((dbf->cap & (DB_CAP_QUERY|DB_CAP_RAW_QUERY)) && (dbf->free_result==0)) {
		LM_ERR("module %s supports queries but does not export free_result\n",
				mname);
		goto error;
	}

	if (dbf->insert) {
		dbf->cap |= DB_CAP_INSERT;
	}

	if (dbf->delete) {
		dbf->cap |= DB_CAP_DELETE;
	}

	if (dbf->update) {
		dbf->cap |= DB_CAP_UPDATE;
	}

	if (dbf->last_inserted_id) {
		dbf->cap |= DB_CAP_LAST_INSERTED_ID;
	}

	if (dbf->insert_update) {
		dbf->cap |= DB_CAP_INSERT_UPDATE;
	}

	if( (dbf->socket && !dbf->resume ) || (!dbf->socket && dbf->resume ) ) {
		LM_ERR("Module does support both socket and resume functions\n");
		goto error;
	}

	return 0;
error:
	return -1;
}



/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_use_table(db_con_t* _h, str* _t)
{
	if (!_h || !_t || !_t->s) {
		LM_ERR("invalid parameter value %p, %p\n", _h, _t);
		return -1;
	}

	CON_TABLE(_h) = _t;
	return 0;
}


int register_module(char * name, db_func_t* dbf)
{
	int ret = db_check_api (dbf, name);

	if( ret)

		return ret;

	db_set_module(name, dbf);

	return 0;
}

void db_assign_ps_idx( db_con_t * conn)
{
	int * ps_idx = conn->ps_idx;

	lock_get(ps_lock);

	if( ps_idx && *ps_idx == 0)
	{
		ps_count ++;
		*ps_idx = ps_count;
	}

	lock_release(ps_lock);
}
