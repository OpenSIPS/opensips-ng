/* 
 * $Id: db_postgres.c 5976 2009-08-18 13:35:23Z bogdan_iancu $ 
 *
 * Postgres module interface
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
 *
 * History:
 * --------
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 */

#include <stdio.h>
#include "../../modules.h"
#include "../../db/db_to_module.h"
#include "dbase.h"


static int mod_init(void);


struct core_module_interface interface = {
	"db_postgres",
	OPENSIPS_FULL_VERSION,       /* compile version */
	OPENSIPS_COMPILE_FLAGS,      /* compile flags */
	CORE_MODULE_UTILS,
	0,
	/* parameters */
	NULL,
	NULL,
	/* functions */
	mod_init,
	NULL,
	NULL,
	NULL
};


static db_func_t dbb =
{
	db_postgres_use_table,     /* Specify table name */
	db_postgres_init,          /* Initialize database connection */
	db_postgres_close,         /* Close database connection */
	db_postgres_query,         /* query a table */
	db_postgres_fetch_row,     /* fetch row */
	db_postgres_raw_query,     /* Raw query - SQL */
	db_postgres_free_result,   /* Free a query result */
	db_postgres_insert,        /* Insert into table */
	db_postgres_delete,        /* Delete from table */
	db_postgres_update,        /* Update table */
	NULL,  /* Retrieve the last inserted IDin a table */
	NULL, /* Insert into table, update on duplicate key */
	db_postgres_socket,
	db_postgres_resume,
	0
};

static int mod_init(void)
{
	LM_INFO("initializing...\n");

	register_module("postgres", &dbb);

	return 0;
}

