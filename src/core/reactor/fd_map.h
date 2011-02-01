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
 *  2010-04-xx  created (adragus)
 */

#ifndef _FD_MAP
#define _FD_MAP

struct _reactor;

#define CALLBACK_COMPLEX_F 1


typedef int (fd_callback)(void *param);
typedef int (fd_callback_complex)(struct _reactor * last_reactor, int fd, void *param);



/*
 * maps a fd to some other structure;
 * used in all cases
 *
 */
struct fd_map
{
	int fd;				/* fd no */
	int priority;		/* task priority */

	int flags;			/* type of data  */
	fd_callback *cb;	/* callback to be trigger when fd is activated */
	void* cb_param;		/* parameter to be passed to callback */
	struct _reactor * last_reactor;

};

#endif
