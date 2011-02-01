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


#ifndef _CORE_SRC_NET_PARAMS_H
#define _CORE_SRC_NET_PARAMS_H

extern int tcp_max_connections;

extern int net_tos;

#ifdef USE_MCAST
extern int mcast_loopback;
extern int mcast_ttl;
#endif /* USE_MCAST */

extern unsigned int udp_maxbuffer;

extern unsigned int udp_min_size;

extern unsigned int tcp_max_size;

extern unsigned int tcp_via_alias;

extern unsigned int tcp_lifetime;

extern unsigned int tcp_write_timeout;

extern unsigned int tcp_connect_timeout;


/* Parses and sets the network TOS - used from cfg
 */
int set_net_tos(char *s);



#endif

