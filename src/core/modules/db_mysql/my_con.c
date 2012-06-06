/* 
 * $Id: my_con.c 5901 2009-07-21 07:45:05Z bogdan_iancu $
 *
 * Copyright (C) 2001-2004 iptel.org
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

#include "my_con.h"
#include "db_mysql.h"
#include "dbase.h"
#include <mysql/mysql_version.h>
#include "../../mem/mem.h"
#include "../../log.h"
#include <string.h>
#include "../../utils.h"


int db_mysql_connect(struct my_con* ptr)
{
	/* if connection already in use, close it first*/
	if (ptr->init)
		mysql_close(ptr->con);

	mysql_init(ptr->con);
	ptr->init = 1;

	if (ptr->id->port) {
		LM_DBG("opening connection: mysql://xxxx:xxxx@%s:%d/%s\n",
			ZSW(ptr->id->host), ptr->id->port, ZSW(ptr->id->database));
	} else {
		LM_DBG("opening connection: mysql://xxxx:xxxx@%s/%s\n",
			ZSW(ptr->id->host), ZSW(ptr->id->database));
	}

	if (!mysql_real_connect(ptr->con, ptr->id->host, 
			ptr->id->username, ptr->id->password,
			ptr->id->database, ptr->id->port, 0,
#if (MYSQL_VERSION_ID >= 40100)
			CLIENT_MULTI_STATEMENTS|CLIENT_REMEMBER_OPTIONS
#else
			CLIENT_REMEMBER_OPTIONS
#endif
	)) {
		LM_ERR("driver error(%d): %s\n",
			mysql_errno(ptr->con), mysql_error(ptr->con));
		mysql_close(ptr->con);
		return -1;
	}
	/* force no auto reconnection */
	ptr->con->reconnect = 0;

	LM_DBG("connection type is %s\n", mysql_get_host_info(ptr->con));
	LM_DBG("protocol version is %d\n", mysql_get_proto_info(ptr->con));
	LM_DBG("server version is %s\n", mysql_get_server_info(ptr->con));

	return 0;
}




/**
 * Close the connection and release memory
 */
void db_mysql_free_connection(void)
{
	//TODO
}
