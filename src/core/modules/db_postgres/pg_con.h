/*
 * $Id: pg_con.h 5898 2009-07-20 16:41:39Z bogdan_iancu $
 *
 * POSTGRES module, portions of this code were templated using
 * the mysql module, thus it's similarity.
 *
 * Copyright (C) 2003 August.Net Services, LLC
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


#ifndef PG_CON_H
#define PG_CON_H

#include "../../db/db_id.h"

#include <time.h>
#include <libpq-fe.h>

/*
 * Postgres specific connection data
 */
struct pg_con {
	struct db_id* id;        /* Connection identifier */
	unsigned int ref;        /* Reference count */

	int connected;
	char *sqlurl;		/* the url we are connected to, all connection memory parents from this */
	PGconn *con;		/* this is the postgres connection */

};

#define CON_SQLURL(db_con)     (((struct pg_con*)((db_con)->tail))->sqlurl)
#define CON_CONNECTION(db_con) (((struct pg_con*)((db_con)->tail))->con)
#define CON_CONNECTED(db_con)  (((struct pg_con*)((db_con)->tail))->connected)
#define CON_ID(db_con) 	       (((struct pg_con*)((db_con)->tail))->id)



#endif /* PG_CON_H */
