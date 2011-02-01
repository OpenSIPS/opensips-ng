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
 *  2010-01-xx  created (bogdan)
 */


#ifndef _H_CORE_GLOBALS
#define _H_CORE_GLOBALS

#include "reactor/reactor.h"

extern int dns_try_ipv6;

extern reactor_t *reactor_in;
extern reactor_t *reactor_out;

extern time_t startup_time;

extern int osips_argc;

extern char **osips_argv;

extern int tcp_disable;
extern int tls_disable;
extern int sctp_disable;


#endif

