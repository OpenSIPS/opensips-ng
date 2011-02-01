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


#ifndef _DB_GLOBALS
#define _DB_GLOBALS

#include "../locking/locking.h"
#include <semaphore.h>
#include "db_core.h"

/* data used for prepared statements */
extern int ps_count;
extern gen_lock_t * ps_lock;

/* the queue of query/item pairs waiting to be issued */
extern db_query_t * waiting;
extern sem_t w_sem;
extern gen_lock_t * w_lock;

/* the list of initialized connection pools */
extern db_pool_t * pool_list;
extern gen_lock_t * db_pools_lock;

/* lock that protects registering/finding modules */
extern gen_lock_t * modules_lock;

#endif
