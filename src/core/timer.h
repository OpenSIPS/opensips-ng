/*
 * Copyright (C) 2010 OpenSIPS Project
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
 *  2010-03-26  created (bogdan)
 */

#ifndef  _CORE_TIMER_H
#define  _CORE_TIMER_H


#define TIMER_TICK   1  				/*!< one second */
#define UTIMER_TICK  100*1000			/*!< 100 miliseconds*/

typedef fd_callback timer_function;

typedef unsigned long long utime_t;

unsigned int get_ticks(void);

utime_t get_uticks(void);

int register_timer( timer_function f, void *param, unsigned int interval);

int register_utimer( timer_function f, void *param, unsigned int interval);

int start_timer_thread(void);

void destroy_timer(void);

#endif

