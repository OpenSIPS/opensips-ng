/* 
 * $Id: db_mysql.c 5976 2009-08-18 13:35:23Z bogdan_iancu $ 
 *
 * MySQL module interface
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
/*
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#include "../../db/db_to_module.h"
#include "dbase.h"
#include "db_mysql.h"
#include "../../modules.h"

#include <string.h>
#include <mysql/mysql.h>

unsigned int db_mysql_ping_interval = 5 * 60; /* Default is 5 minutes */
unsigned int db_mysql_timeout_interval = 6;   /* Default is 6 seconds */

static db_func_t dbb =
{
	db_mysql_use_table,     /* Specify table name */
	db_mysql_init,          /* Initialize database connection */
	db_mysql_close,         /* Close database connection */
	db_mysql_query,         /* query a table */
	db_mysql_fetch_next_row,/* fetch next row */
	db_mysql_raw_query,     /* Raw query - SQL */
	db_mysql_free_result,   /* Free a query result */
	db_mysql_insert,        /* Insert into table */
	db_mysql_delete,        /* Delete from table */
	db_mysql_update,        /* Update table */
	db_mysql_last_inserted_id,  /* Retrieve the last inserted ID in a table */
	db_insert_update_func, /* Insert into table, update on duplicate key */
	db_mysql_socket,
	db_mysql_resume,
	DB_CAP_SYNC_PREP_STMT
};


static int mysql_mod_init(void);
static void mysql_mod_destroy(void);

static config_param_t module_params[] = {
	{"ping_interval", &db_mysql_ping_interval, PARAM_TYPE_INT, 0},
	{"timeout_interval",&db_mysql_timeout_interval, PARAM_TYPE_INT, 0},
	{0, 0, 0, 0}
};


struct core_module_interface interface = {
	"db_mysql",
	OPENSIPS_FULL_VERSION,       /* compile version */
	OPENSIPS_COMPILE_FLAGS,      /* compile flags */
	CORE_MODULE_UTILS,
	0,
	/* parameters */
	module_params,
	NULL,
	/* functions */
	mysql_mod_init,
	mysql_mod_destroy,
	NULL,
	NULL
};


static int mysql_mod_init(void)
{
	LM_DBG("mysql: MySQL client version is %s\n", mysql_get_client_info());


	if( mysql_thread_safe() !=1 )
	{
		LM_ERR("MySQL client is not compiled as thread-safe\n");
		return -1;
	}

	if( mysql_library_init(0, NULL, NULL) )
	{
		LM_ERR("Unable to initialize MySQL library\n");
		return -1;
	}

	register_module("mysql", &dbb);
	return 0;
}

static void mysql_mod_destroy(void)
{
	mysql_library_end();
}
