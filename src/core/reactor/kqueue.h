/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 */ 


#ifdef HAVE_KQUEUE
#ifndef _KQUEUE_ASYNC
#define _KQUEUE_ASYNC


#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include "general.h"

int kqueue_init(io_wait_h *h);
int kqueue_loop(io_wait_h *h, int t);
int kqueue_del(io_wait_h *h, int fd);
int kqueue_add(io_wait_h* h, int fd, int type, int priority,
		fd_callback *cb, void *cb_param);
void kqueue_destroy(io_wait_h *h);

#endif
#endif
