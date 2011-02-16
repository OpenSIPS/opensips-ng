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

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <strings.h>

#include "../log.h"


int tcp_max_connections = 2048;

int net_tos = IPTOS_LOWDELAY;

#ifdef USE_MCAST
int mcast_loopback = 0;
int mcast_ttl = -1; /* if -1, don't touch it, use the default (usually 1) */
#endif /* USE_MCAST */

/* maximum buffer size we do not want to exceed during the auto-probing
 * procedure; may be re-configured */
unsigned int udp_maxbuffer = 256*1024;

/* minimum amount of data to be considered a package */
unsigned int udp_min_size = 21;

/* maximum size of the SIP message over TCP */
unsigned int tcp_max_size = 512*1024;

/* if VIA port should alias the TCP connection */
unsigned int tcp_via_alias = 1;

unsigned int tcp_lifetime = 10 * 60; /* 10 minutes */

unsigned int tcp_write_timeout = 10 ; /* 10 seconds */

unsigned int tcp_connect_timeout = 10 ; /* 10 seconds */


int set_net_tos(char *s)
{
	if (strcasecmp(s,"IPTOS_LOWDELAY")==0) {
		net_tos=IPTOS_LOWDELAY;
	} else if (strcasecmp(s,"IPTOS_THROUGHPUT")==0) {
		net_tos=IPTOS_THROUGHPUT;
	} else if (strcasecmp(s,"IPTOS_RELIABILITY")==0) {
		net_tos=IPTOS_RELIABILITY;
	#if defined(IPTOS_MINCOST)
	} else if (strcasecmp(s,"IPTOS_MINCOST")==0) {
		net_tos=IPTOS_MINCOST;
	#endif
	#if defined(IPTOS_LOWCOST)
	} else if (strcasecmp(s,"IPTOS_LOWCOST")==0) {
		net_tos=IPTOS_LOWCOST;
	#endif
	} else {
		LM_ERR("invalid tos value %s - allowed: "
			"IPTOS_LOWDELAY,IPTOS_THROUGHPUT,"
			"IPTOS_RELIABILITY"
			#if defined(IPTOS_LOWCOST)
			",IPTOS_LOWCOST"
			#endif
			#if defined(IPTOS_MINCOST)
			",IPTOS_MINCOST"
			#endif
			"\n", s);
		return -1;
	}

	return 0;
}


