/*
 * Copyright (C) 2009 OpenSIPS Project
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

#ifndef _DB_PS_H
#define _DB_PS_H

typedef void * db_ps_t;

/** Is any prepared statement provided for the next query? */
#define CON_HAS_PS(cn)  ((cn)->ps_idx)

/** Pointer to the current used prepared statment */
#define CON_CURR_PS(cn)      ((cn)->statements[*cn->ps_idx])

/** Does the connection have attached an uninitialized prepared statement? */
#define CON_HAS_UNINIT_PS(cn)  ( CON_CURR_PS(cn) == NULL )

/** Pointer to the address of the current used prepared statment */
#define CON_PS_REFERENCE(cn)      ( &CON_CURR_PS(cn) )

//#define CON_RESET_CURR_PS(cn)    * CON_PS_REFERENCE(cn) = NULL
#define CON_RESET_CURR_PS(cn)    ((cn)->ps_idx) = NULL

#endif


