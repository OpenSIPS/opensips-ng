/*
 * $Id: dbase.c 5898 2009-07-20 16:41:39Z bogdan_iancu $
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
 * 2006-07-28 within pg_get_result(): added check to immediatly return of no 
 *            result set was returned added check to only execute 
 *            convert_result() if PGRES_TUPLES_OK added safety check to avoid 
 *            double pg_free_result() (norm)
 * 2006-08-07 Rewrote pg_get_result().
 *            Additional debugging lines have been placed through out the code.
 *            Added Asynchronous Command Processing (PQsendQuery/PQgetResult) 
 *            instead of PQexec. this was done in preparation of adding FETCH 
 *            support.  Note that PQexec returns a result pointer while 
 *            PQsendQuery does not.  The result set pointer is obtained from 
 *            a call (or multiple calls) to PQgetResult.
 *            Removed transaction processing calls (BEGIN/COMMIT/ROLLBACK) as 
 *            they added uneeded overhead.  Klaus' testing showed in excess of 
 *            1ms gain by removing each command.  In addition, OpenSIPS only 
 *            issues single queries and is not, at this time transaction aware.
 *            The transaction processing routines have been left in place 
 *            should this support be needed in the future.
 *            Updated logic in pg_query / pg_raw_query to accept a (0) result 
 *            set (_r) parameter.  In this case, control is returned
 *            immediately after submitting the query and no call to 
 *            pg_get_results() is performed. This is a requirement for 
 *            FETCH support. (norm)
 * 2006-10-27 Added fetch support (norm)
 *            Removed dependency on aug_* memory routines (norm)
 *            Added connection pooling support (norm)
 *            Standardized API routines to pg_* names (norm)
 * 2006-11-01 Updated pg_insert(), pg_delete(), pg_update() and 
 *            pg_get_result() to handle failed queries.  Detailed warnings 
 *            along with the text of the failed query is now displayed in the 
 *            log. Callers of these routines can now assume that a non-zero 
 *            rc indicates the query failed and that remedial action may need 
 *            to be taken. (norm)
 */

#define MAXCOLUMNS	512

#include <string.h>
#include <stdio.h>
#include "../../log.h"
#include "../../mem/mem.h"
#include "../../db/db_ut.h"
#include "../../db/db_query.h"
#include "dbase.h"
#include "pg_con.h"
#include "val.h"
#include "res.h"


void* db_postgres_init(struct db_id* id)
{
	struct pg_con* ptr;
	char *ports;

	LM_DBG("db_id = %p\n", id);

	if (!id)
	{
		LM_ERR("invalid db_id parameter value\n");
		return 0;
	}

	ptr = (struct pg_con*) pkg_malloc(sizeof (struct pg_con));
	if (!ptr)
	{
		LM_ERR("failed trying to allocated %lu bytes for connection structure."
			"\n", (unsigned long) sizeof (struct pg_con));
		return 0;
	}
	LM_DBG("%p=pkg_malloc(%lu)\n", ptr, (unsigned long) sizeof (struct pg_con));

	memset(ptr, 0, sizeof (struct pg_con));
	ptr->ref = 1;

	if (id->port)
	{
		ports = int2str(id->port, 0);
		LM_DBG("opening connection: postgres://xxxx:xxxx@%s:%d/%s\n", ZSW(id->host),
			id->port, ZSW(id->database));
	} else
	{
		ports = NULL;
		LM_DBG("opening connection: postgres://xxxx:xxxx@%s/%s\n", ZSW(id->host),
			ZSW(id->database));
	}

	ptr->con = PQsetdbLogin(id->host, ports, NULL, NULL, id->database, id->username, id->password);
	LM_DBG("PQsetdbLogin(%p)\n", ptr->con);

	if ((ptr->con == 0) || (PQstatus(ptr->con) != CONNECTION_OK))
	{
		LM_ERR("%s\n", PQerrorMessage(ptr->con));
		PQfinish(ptr->con);
		goto err;
	}

	PQsetnonblocking(ptr->con, 1);
	
	ptr->connected = 1;
	ptr->id = id;

	return ptr;

err:
	if (ptr)
	{
		LM_ERR("cleaning up %p=pkg_free()\n", ptr);
		pkg_free(ptr);
	}
	return 0;
}

/*
 ** pg_close	last function to call when db is no longer needed
 **
 **	Arguments :
 **		db_con_t * the connection to shut down, as supplied by pg_init()
 **
 **	Returns :
 **		(void)
 **
 **	Notes :
 **		All memory and resources are freed.
 */

void db_postgres_close(db_con_t* _h)
{

	struct pg_con * _c;
	
	_c = (struct pg_con*)_h->tail;

	if (_c->id) free_db_id(_c->id);
	if (_c->con) {
		LM_DBG("PQfinish(%p)\n", _c->con);
		PQfinish(_c->con);
		_c->con = 0;
	}
	LM_DBG("pkg_free(%p)\n", _c);
	pkg_free(_c);

}

/*
 ** submit_query	run a query
 **
 **	Arguments :
 **		db_con_t *	as previously supplied by pg_init()
 **		char *_s	the text query to run
 **
 **	Returns :
 **		0 upon success
 **		negative number upon failure
 */

static int db_postgres_submit_query(db_con_t* _con, str* _s)
{
	if (!_con || !_s || !_s->s)
	{
		LM_ERR("invalid parameter value\n");
		return (-1);
	}

	/* this bit of nonsense in case our connection get screwed up */
	switch (PQstatus(CON_CONNECTION(_con)))
	{
	case CONNECTION_OK:
		break;
	case CONNECTION_BAD:
		LM_DBG("connection reset\n");
		PQreset(CON_CONNECTION(_con));
		break;
	case CONNECTION_STARTED:
	case CONNECTION_MADE:
	case CONNECTION_AWAITING_RESPONSE:
	case CONNECTION_AUTH_OK:
	case CONNECTION_SETENV:
	case CONNECTION_SSL_STARTUP:
	case CONNECTION_NEEDED:
	default:
		LM_ERR("%p PQstatus(%s) invalid: %.*s\n", _con,
			   PQerrorMessage(CON_CONNECTION(_con)), _s->len, _s->s);
		return -1;
	}


	/* exec the query */
	if (PQsendQuery(CON_CONNECTION(_con), _s->s))
	{
		LM_DBG("%p PQsendQuery(%.*s)\n", _con, _s->len, _s->s);
	} else
	{
		LM_ERR("%p PQsendQuery Error: %s Query: %.*s\n", _con,
			PQerrorMessage(CON_CONNECTION(_con)), _s->len, _s->s);
		return -1;
	}

	return 0;
}

/*
 ** db_free_result	free the query and free the result memory
 **
 **	Arguments :
 **		db_con_t *	as previously supplied by pg_init()
 **		db_res_t *	the result of a query
 **
 **	Returns :
 **		0 upon success
 **		negative number upon failure
 */

void db_postgres_free_result(void * data)
{
	LM_DBG("PQclear(%p) result set\n", data);
	PQclear(data);
}

/*
 * Query table for specified rows
 * _con: structure representing database connection
 * _k: key names
 * _op: operators
 * _v: values of the keys that must match
 * _c: column names to return
 * _n: nmber of key=values pairs to compare
 * _nc: number of columns to return
 * _o: order by the specified column
 */
int db_postgres_query(db_con_t* _h, db_key_t* _k,
					  db_op_t* _op, db_val_t* _v, db_key_t* _c, int _n,
					  int _nc, db_key_t _o, db_res_t** _r)
{
	int ret = db_do_query(_h, _k, _op, _v, _c, _n, _nc, _o, NULL,
					db_postgres_val2str, db_postgres_submit_query,
					db_postgres_store_result,NULL);

	PQflush(CON_CONNECTION(_h));

	return ret;

}

int db_postgres_fetch_row(db_res_t *_r,db_row_t* _rw)
{
	char **row_buf,*s;
	int col,len,row;
	db_con_t *_h = _r->it->connection;
	row = _r->next_row;

	len = sizeof(char *) * RES_COL_N(_r);
	row_buf = (char **)pkg_malloc(len);
	if (!row_buf) 
	{
		LM_ERR("no private memory left\n");
		return -1;
	}

	memset(row_buf, 0, len);

	for(col = 0; col < RES_COL_N(_r); col++) {
		/*
		 * The row data pointer returned by PQgetvalue points to 
		 * storage that is part of the PGresult structure. One should 
		 * not modify the data it points to, and one must explicitly 
		 * copy the data into other storage if it is to be used past 
		 * the lifetime of the PGresult structure itself.
		 */
		
		/*
		 * There's a weird bug (or just weird behavior) in the postgres
		 * API - if the result is a BLOB (like 'text') and is with 
		 * zero length, we get a pointer to nowhere, which is not 
		 * null-terminated. The fix for this is to check what does the 
		 * DB think about the length and use that as a correction.
		 */
		if (PQgetisnull(_r->data, row, col) == 0) {
			/* not null value */
			if ( (len=PQgetlength(_r->data, row, col))==0 ) {
				s="";
				LM_DBG("PQgetvalue(%p,%d,%d)=[], zero len\n", _h, row,col);
			} else {
				s = PQgetvalue(_r->data, row, col);
				LM_DBG("PQgetvalue(%p,%d,%d)=[%.*s]\n", _h, row,col,len,s);
				
				row_buf[col] = pkg_malloc(len+1);
				if (!row_buf[col]) {
					LM_ERR("no private memory left\n");
					return -1;
				}
				memset(row_buf[col], 0, len+1);
				LM_DBG("allocated %d bytes for row_buf[%d] at %p\n",
					len, col, row_buf[col]);

				strncpy(row_buf[col], s, len);
				LM_DBG("[%d][%d] Column[%.*s]=[%s]\n",
					row, col, RES_NAMES(_r)[col]->len,
					RES_NAMES(_r)[col]->s, row_buf[col]);
			}
		}
	}
	/* ASSERT: row_buf contains an entire row in strings */
	if(db_postgres_convert_row(_h, _r, _rw, row_buf)<0)
	{
		LM_ERR("failed to convert row #%d\n",  row);
		for (col = 0; col < RES_COL_N(_r); col++) {
			LM_DBG("freeing row_buf[%d] at %p\n", col, row_buf[col]);
			if (row_buf[col] &&  !row_buf[col][0]) pkg_free(row_buf[col]);
		}
		LM_DBG("freeing row buffer at %p\n", row_buf);
		pkg_free(row_buf);
		return -4;
	}
	/*
	 * pkg_free() must be done for the above allocations now that the row
	 * has been converted. During pg_convert_row (and subsequent pg_str2val)
	 * processing, data types that don't need to be converted (namely STRINGS
	 * and STR) have their addresses saved. These data types should not have
	 * their pkg_malloc() allocations freed here because they are still
	 * needed.  However, some data types (ex: INT, DOUBLE) should have their
	 * pkg_malloc() allocations freed because during the conversion process,
	 * their converted values are saved in the union portion of the db_val_t
	 * structure. BLOB will be copied during PQunescape in str2val, thus it
	 * has to be freed here AND in pg_free_row().
	 *
	 * Warning: when the converted row is no longer needed, the data types
	 * whose addresses were saved in the db_val_t structure must be freed
	 * or a memory leak will happen. This processing should happen in the
	 * pg_free_row() subroutine. The caller of this routine should ensure
	 * that pg_free_rows(), pg_free_row() or pg_free_result() is eventually
	 * called.
	 */
	for (col = 0; col < RES_COL_N(_r); col++) {
		switch (RES_TYPES(_r)[col]) {
			case DB_STRING:
			case DB_STR:
				break;
			default:
				LM_DBG("freeing row_buf[%d] at %p\n", col, row_buf[col]);
				if (row_buf[col]) pkg_free(row_buf[col]);
		}
		/*
		 * The following housekeeping may not be technically required, but it
		 * is a good practice to NULL pointer fields that are no longer valid.
		 * Note that DB_STRING fields have not been pkg_free(). NULLing DB_STRING
		 * fields would normally not be good to do because a memory leak would
		 * occur.  However, the pg_convert_row() routine  has saved the DB_STRING
		 * pointer in the db_val_t structure.  The db_val_t structure will 
		 * eventually be used to pkg_free() the DB_STRING storage.
		 */
		row_buf[col] = (char *)NULL;
	}

	pkg_free(row_buf);
	_r->next_row++;
	return 0;
}

/*
 * Execute a raw SQL query
 */
int db_postgres_raw_query(db_con_t* _h, str* _s, db_res_t** _r)
{
	int ret = db_do_raw_query(_h, _s, NULL, db_postgres_submit_query,
						db_postgres_store_result);

	PQflush(CON_CONNECTION(_h));
	return ret;
}

/*
 * Retrieve result set
 *
 * Input:
 *   db_con_t*  _con Structure representing the database connection
 *   db_res_t** _r pointer to a structure represending the result set
 *
 * Output:
 *   return 0: If the status of the last command produced a result set and,
 *   If the result set contains data or the convert_result() routine
 *   completed successfully.
 *
 *   return < 0: If the status of the last command was not handled or if the
 *   convert_result() returned an error.
 *
 * Notes:
 *   A new result structure is allocated on every call to this routine.
 *
 *   If this routine returns 0, it is the callers responsbility to free the
 *   result structure. If this routine returns < 0, then the result structure
 *   is freed before returning to the caller.
 *
 */

int db_postgres_store_result(db_con_t* _con, db_res_t** _r)
{
	PGresult *res = NULL, *tmp = NULL;
	ExecStatusType pqresult;
	int rc = 0;
	db_res_t* r;

	while (1)
	{
		if ((tmp = PQgetResult(CON_CONNECTION(_con))))
		{
			res = tmp;
		} else
		{
			break;
		}
	}

	pqresult = PQresultStatus(res);

	LM_DBG("%p PQresultStatus(%s) PQgetResult(%p)\n", _con,
		PQresStatus(pqresult), res);

	switch (pqresult)
	{
	case PGRES_COMMAND_OK:
		/* Successful completion of a command returning no data
		 * (such as INSERT or UPDATE). */
		rc = 0;
		break;

	case PGRES_TUPLES_OK:
		/* Successful completion of a command returning data
		 * (such as a SELECT or SHOW). */
		*_r = db_new_result();
		if (*_r == NULL)
		{
			LM_ERR("failed to init new result\n");
			rc = -1;
			goto done;
		}

		r = *_r;
		r->data = res;
		
		if (db_postgres_convert_result(_con, *_r) < 0)
		{
			LM_ERR("%p Error returned from convert_result()\n", _con);
			db_mem_free_result(*_r);
			*_r = 0;
			rc = -4;
			break;
		}
		return 0;
		
		/* query failed */
	case PGRES_FATAL_ERROR:
		LM_ERR("%p - invalid query, execution aborted\n", _con);
		LM_ERR("%p: %s\n", _con, PQresStatus(pqresult));
		LM_ERR("%p: %s\n", _con, PQresultErrorMessage(res));
		*_r = 0;
		rc = -3;
		break;

	case PGRES_EMPTY_QUERY:
		/* notice or warning */
	case PGRES_NONFATAL_ERROR:
		/* status for COPY command, not used */
	case PGRES_COPY_OUT:
	case PGRES_COPY_IN:
		/* unexpected response */
	case PGRES_BAD_RESPONSE:
	default:
		LM_ERR("%p Probable invalid query\n", _con);
		LM_ERR("%p: %s\n", _con, PQresStatus(pqresult));
		LM_ERR("%p: %s\n", _con, PQresultErrorMessage(res));
		*_r = 0;
		rc = -4;
		break;
	}

done:

	PQclear(res);
	return (rc);
}

/*
 * Insert a row into specified table
 * _con: structure representing database connection
 * _k: key names
 * _v: values of the keys
 * _n: number of key=value pairs
 */
int db_postgres_insert(db_con_t* _h, db_key_t* _k,
					   db_val_t* _v, int _n)
{
	int tmp = db_do_insert(_h, _k, _v, _n, db_postgres_val2str,
						db_postgres_submit_query,NULL);

	while( PQflush(CON_CONNECTION(_h)) );

	return tmp;
}

/*
 * Delete a row from the specified table
 * _con: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _n: number of key=value pairs
 */
int db_postgres_delete(db_con_t* _h, db_key_t* _k,
					   db_op_t* _o, db_val_t* _v, int _n)
{
	int tmp = db_do_delete(_h, _k, _o, _v, _n, db_postgres_val2str,
						db_postgres_submit_query,NULL);

	while( PQflush(CON_CONNECTION(_h)) );

	return tmp;
}

/*
 * Update some rows in the specified table
 * _con: structure representing database connection
 * _k: key names
 * _o: operators
 * _v: values of the keys that must match
 * _uk: updated columns
 * _uv: updated values of the columns
 * _n: number of key=value pairs
 * _un: number of columns to update
 */
int db_postgres_update(db_con_t* _h, db_key_t* _k,
					   db_op_t* _o, db_val_t* _v, db_key_t* _uk,
					   db_val_t* _uv, int _n, int _un)
{
	int tmp = db_do_update(_h, _k, _o, _v, _uk, _uv, _n, _un,
						db_postgres_val2str, db_postgres_submit_query,NULL);

	while( PQflush(CON_CONNECTION(_h)) );

	return tmp;
}

/*
 * Store name of table that will be used by
 * subsequent database functions
 */
int db_postgres_use_table(db_con_t* _con, str* _t)
{
	return db_use_table(_con, _t);
}

int db_postgres_socket (db_con_t* h)
{
	return PQsocket(CON_CONNECTION(h));
}

int db_postgres_resume (  db_con_t* h, db_res_t ** r)
{
	if( PQconsumeInput(CON_CONNECTION(h)) == 0)
	{
		LM_ERR("Unable to consume input\n");
		return -1;
	}

	if(PQisBusy(CON_CONNECTION(h)) )
	{
		return 1;
	}
	else
	{
		db_postgres_store_result(h, r);
		return 0;
	}
}
