/*
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

/**

 */

#ifndef DB_H
#define DB_H

#include "db_key.h"
#include "db_op.h"
#include "db_val.h"
#include "db_con.h"
#include "db_res.h"
#include "db_cap.h"
#include "db_con.h"
#include "db_row.h"
#include "db_ps.h"
#include "db_core.h"


/**
 * \brief Specify table name that will be used for subsequent operations.
 * 
 * The function db_use_table takes a table name and stores it db_con_t structure.
 * All subsequent operations (insert, delete, update, query) are performed on
 * that table.
 * \param _h database connection handle
 * \param _t table name
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_use_table_f)(db_con_t* _h,   str * _t);

/**
 * \brief Initialize database connection and obtain the connection handle.
 *
 * This function initialize the database API and open a new database
 * connection. This function must be called after bind_dbmod but before any
 * other database API function is called.
 * 
 * The function takes one parameter, the parameter must contain the database
 * connection URL. The URL is of the form 
 * mysql://username:password\@host:port/database where:
 * 
 * username: Username to use when logging into database (optional).
 * password: password if it was set (optional)
 * host:     Hosname or IP address of the host where database server lives (mandatory)
 * port:     Port number of the server if the port differs from default value (optional)
 * database: If the database server supports multiple databases, you must specify the
 * name of the database (optional).
 * \see bind_dbmod
 * \param _sqlurl database connection URL
 * \return returns a pointer to the db_con_t representing the connection if it was
 * successful, otherwise 0 is returned
 */
typedef void* (*db_init_f) (  db_id_t * id);

/**
 * \brief Close a database connection and free all memory used.
 *
 * The function closes previously open connection and frees all previously 
 * allocated memory. The function db_close must be the very last function called.
 * \param _h db_con_t structure representing the database connection
 */
typedef void (*db_close_f) (db_con_t* _h); 


/**
 * \brief Query table for specified rows.
 *
 * This function implements the SELECT SQL directive.
 * If _k and _v parameters are NULL and _n is zero, you will get the whole table.
 *
 * if _c is NULL and _nc is zero, you will get all table columns in the result.
 * _r will point to a dynamically allocated structure, it is neccessary to call
 * db_free_result function once you are finished with the result.
 *
 * If _op is 0, equal (=) will be used for all key-value pairs comparisons.
 *
 * Strings in the result are not duplicated, they will be discarded if you call
 * db_free_result, make a copy yourself if you need to keep it after db_free_result.
 *
 * You must call db_free_result before you can call db_query again!
 * \see db_free_result
 *
 * \param _h database connection handle
 * \param _k array of column names that will be compared and their values must match
 * \param _op array of operators to be used with key-value pairs
 * \param _v array of values, columns specified in _k parameter must match these values
 * \param _c array of column names that you are interested in
 * \param _n number of key-value pairs to match in _k and _v parameters
 * \param _nc number of columns in _c parameter
 * \param _o order by statement for query
 * \param _r address of variable where pointer to the result will be stored
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_select_f) (  db_con_t* _h,   db_key_t* _k,   db_op_t* _op,
				  db_val_t* _v,   db_key_t* _c,   int _n,   int _nc,
				  db_key_t _o, db_res_t** _r);

/**
 * \brief Fetch a number of rows from a result.
 * 
 * The function fetches a number of rows from a database result. If the number
 * of wanted rows is zero, the function returns anything with a result of zero.
 * \param _h structure representing database connection
 * \param _r structure for the result
 * \param _n the number of rows that should be fetched
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_fetch_next_row_f) (db_res_t* _res,db_row_t *_r);


/**
 * \brief Raw SQL query.
 *
 * This function can be used to do database specific queries. Please
 * use this function only if needed, as this creates portability issues
 * for the different databases. Also keep in mind that you need to
 * escape all external data sources that you use. You could use the
 * escape_common and unescape_common functions in the core for this task.
 * \see escape_common
 * \see unescape_common
 * \param _h structure representing database connection
 * \param _s the SQL query
 * \param _r structure for the result
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_raw_query_f) (  db_con_t* _h,   str* _s, db_res_t** _r);


/**
 * \brief Free a result allocated by db_query.
 *
 * This function frees all memory allocated previously in db_query. Its
 * neccessary to call this function on a db_res_t structure if you don't need the
 * structure anymore. You must call this function before you call db_query again!
 * \param _h database connection handle
 * \param _r pointer to db_res_t structure to destroy
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef void (*db_free_result_f) (void * data);


/**
 * \brief Insert a row into the specified table.
 * 
 * This function implements INSERT SQL directive, you can insert one or more
 * rows in a table using this function.
 * \param _h database connection handle
 * \param _k array of keys (column names) 
 * \param _v array of values for keys specified in _k parameter
 * \param _n number of keys-value pairs int _k and _v parameters
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_insert_f) (  db_con_t* _h,   db_key_t* _k,
				  db_val_t* _v,   int _n);


/**
 * \brief Delete a row from the specified table.
 *
 * This function implements DELETE SQL directive, it is possible to delete one or
 * more rows from a table.
 * If _k is NULL and _v is NULL and _n is zero, all rows are deleted, the
 * resulting table will be empty.
 * If _o is NULL, the equal operator "=" will be used for the comparison.
 * 
 * \param _h database connection handle
 * \param _k array of keys (column names) that will be matched
 * \param _o array of operators to be used with key-value pairs
 * \param _v array of values that the row must match to be deleted
 * \param _n number of keys-value parameters in _k and _v parameters
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_delete_f) (  db_con_t* _h,   db_key_t* _k,   db_op_t* _op,
				  db_val_t* _v,   int _n);


/**
 * \brief Update some rows in the specified table.
 *
 * The function implements UPDATE SQL directive. It is possible to modify one
 * or more rows in a table using this function.
 * \param _h database connection handle
 * \param _k array of keys (column names) that will be matched
 * \param _o array of operators to be used with key-value pairs
 * \param _v array of values that the row must match to be modified
 * \param _uk array of keys (column names) that will be modified
 * \param _uv new values for keys specified in _k parameter
 * \param _n number of key-value pairs in _k and _v parameters
 * \param _un number of key-value pairs in _uk and _uv parameters
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_update_f) (  db_con_t* _h,   db_key_t* _k,   db_op_t* _op,
				  db_val_t* _v,   db_key_t* _uk,   db_val_t* _uv,
				  int _n,   int _un);


/**
 * \brief Retrieve the last inserted ID in a table.
 *
 * The function returns the value generated for an AUTO_INCREMENT column by the
 * previous INSERT or UPDATE  statement. Use this function after you have 
 * performed an INSERT statement into a table that contains an AUTO_INCREMENT
 * field.
 * \param _h structure representing database connection
 * \return returns the ID as integer or returns 0 if the previous statement
 * does not use an AUTO_INCREMENT value.
 */
typedef int (*db_last_inserted_id_f) (  db_con_t* _h);


/**
 * \brief Insert a row into specified table, update on duplicate key.
 * 
 * The function implements the INSERT ON DUPLICATE KEY UPDATE SQL directive.
 * It is possible to insert a row and update if one already exists.
 * The old row will not deleted before the insertion of the new data.
 * \param _h structure representing database connection
 * \param _k key names
 * \param _v values of the keys
 * \param _n number of key=value pairs
 * \return returns 0 if everything is OK, otherwise returns value < 0
 */
typedef int (*db_insert_update_f) (  db_con_t* _h,   db_key_t* _k,
				  db_val_t* _v,   int _n);



typedef int (*db_socket_f) (  db_con_t* _h);

/* Callback to continue query execution
 *
 * returns	0 if query is finished
 *			1 if there is more data to be read
 *			-1 on error
 */
typedef int (*db_resume_f) (  db_con_t* _h, db_res_t ** r);


/**
 * \brief Database module callbacks
 * 
 * This structure holds function pointer to all database functions. Before this
 * structure can be used it must be initialized with bind_dbmod.
 * \see bind_dbmod
 */
typedef struct db_func {
	db_use_table_f			use_table;     /* Specify table name */
	db_init_f				init;          /* Initialize database connection */
	db_close_f				close;         /* Close database connection */
	db_select_f				select;        /* query a table */
	db_fetch_next_row_f		fetch_next_row;/* fetch result */
	db_raw_query_f			raw_query;     /* Raw query - SQL */
	db_free_result_f		free_result;   /* Free a query result */
	db_insert_f				insert;        /* Insert into table */
	db_delete_f				delete;        /* Delete from table */ 
	db_update_f				update;        /* Update table */
	db_last_inserted_id_f	last_inserted_id;  /* Retrieve the last inserted ID
	                                            in a table */
	db_insert_update_f insert_update; /* Insert into table, update on 
	                                     duplicate key */ 
	db_socket_f	socket;
	db_resume_f resume;

	unsigned int cap;  /* Mask of capabilities, is filled automatically 
	                      on registration*/
} db_func_t;


int register_module(  char * name, db_func_t* dbf);



/**
 * Method that assigns a new index for the given prepared statement.
 */
void db_assign_ps_idx( db_con_t * conn);


/**
 * \brief Get the version of a table.
 *
 * Returns the version number of a given table from the version table.
 * Instead of this function you could also use db_check_table_version
 * \param dbf database module callbacks
 * \param con database connection handle
 * \param table checked table
 * \return the version number if present, 0 if no version data available, < 0 on error
 */
int db_table_version(  db_func_t* dbf, db_con_t* con,   str* table);


/**
 * \brief Check the table version
 *
 * Small helper function to check the table version.
 * \param dbf database module callbacks
 * \param dbh database connection handle
 * \param table checked table
 * \param \version checked version
 * \return 0 means ok, -1 means an error occured
 */
int db_check_table_version(db_func_t* dbf, db_con_t* dbh,   str* table,   unsigned int version);


/**
 * \brief Stores the name of a table.
 *
 * Stores the name of the table that will be used by subsequent database
 * functions calls in a db_con_t structure.
 * \param _h database connection handle
 * \param _t stored name
 * \return 0 if everything is ok, otherwise returns value < 0
 */
int db_use_table(db_con_t* _h,   str* _t);


/**
 * \brief Bind the DB API exported by a module.
 *
 * The function links the functions implemented by the module to the members
 * of db_func_t structure
 * \param dbb db_func_t structure representing the variable where to bind
 * \return 0 if everything is ok, otherwise returns -1
 */
typedef int (*db_bind_api_f)(  str* mod, db_func_t *dbb);

#endif /* DB_H */
