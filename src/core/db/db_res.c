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
 * \file db/db_res.c
 * \brief Functions to manage result structures
 *
 * Provides some convenience macros and some memory management
 * functions for result structures.
 */

#include "db_res.h"

#include "db_row.h"
#include "../log.h"
#include "../mem/mem.h"

#include <string.h>

/*
 * Release memory used by columns
 */
int db_free_columns(db_res_t* _r)
{
	if (!_r) {
		LM_ERR("invalid parameter value\n");
		return -1;
	}
	/* free names and types */
	if (RES_NAMES(_r)) {
		LM_DBG("freeing result columns at %p\n", RES_NAMES(_r));
		pkg_free(RES_NAMES(_r));
		RES_NAMES(_r) = NULL;
	}
	return 0;
}

/*
 * Create a new result structure and initialize it
 */
db_res_t* db_new_result(void)
{
	db_res_t* r = NULL;
	r = (db_res_t*)pkg_malloc(sizeof(db_res_t));
	if (!r) {
		LM_ERR("no private memory left\n");
		return 0;
	}
	LM_DBG("allocate %d bytes for result set at %p\n",
		(int)sizeof(db_res_t), r);
	memset(r, 0, sizeof(db_res_t));
	return r;
}

/*
 * Release memory used by a result structure
 */
int db_mem_free_result(db_res_t* _r)
{
	if (!_r)
	{
		LM_ERR("invalid parameter\n");
		return -1;
	}

	db_free_columns(_r);
//	db_free_rows(_r);
	LM_DBG("freeing result set at %p\n", _r);
	pkg_free(_r);
	_r = NULL;
	return 0;
}

/*
 * Allocate storage for column names and type in existing
 * result structure.
 */
int db_allocate_columns(db_res_t* _r, const unsigned int cols)
{
	unsigned int i;

	RES_NAMES(_r) = (db_key_t*)pkg_malloc
		( cols * (sizeof(db_key_t)+sizeof(db_type_t)+sizeof(str)) );
	if (!RES_NAMES(_r)) {
		LM_ERR("no private memory left\n");
		return -1;
	}
	LM_DBG("allocate %d bytes for result columns at %p\n",
		(int)(cols * (sizeof(db_key_t)+sizeof(db_type_t)+sizeof(str))),
		RES_NAMES(_r));

	for ( i=0 ; i<cols ; i++)
		RES_NAMES(_r)[i] = (str*)(RES_NAMES(_r)+cols)+i;

	RES_TYPES(_r) = (db_type_t*)(RES_NAMES(_r)[0]+cols);

	return 0;
}

db_row_t* db_allocate_row(int cols)
{
	int size;
	db_row_t* new;

	size = sizeof(db_row_t) + sizeof(db_val_t) * cols;

	new = pkg_malloc(size);
	if (!new)
	{
		LM_ERR("no more memory left \n");
		return NULL;
	}

	memset(new,0,size);
	new->values = (db_val_t *)((char *)new + sizeof(db_row_t));
	new->n = cols;

	return new;
}
