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


#ifndef _CORE_NET_PROTO_H
#define _CORE_NET_PROTO_H

#include "../log.h"


#define PROTO_NONE   0
#define PROTO_UDP    1
#define PROTO_TCP    2
#define PROTO_TLS    3
#define PROTO_SCTP   4
#define PROTO_MAX    5  /* keep this at the end ! */

#define MAX_PROTO_STR_LEN   4


static inline char* proto2str(int proto, char *p)
{
	switch (proto) {
		case PROTO_UDP:
			*(p++) = 'u';
			*(p++) = 'd';
			*(p++) = 'p';
			break;
		case PROTO_TCP:
			*(p++) = 't';
			*(p++) = 'c';
			*(p++) = 'p';
			break;
		case PROTO_TLS:
			*(p++) = 't';
			*(p++) = 'l';
			*(p++) = 's';
			break;
		case PROTO_SCTP:
			*(p++) = 's';
			*(p++) = 'c';
			*(p++) = 't';
			*(p++) = 'p';
			break;
		default:
			LM_CRIT("unsupported proto %d\n", proto);
			return 0;
	}
	return p;
}


/*! \brief Sets protocol
 * \return -1 on error, 0 on success
 */
inline static int parse_proto(unsigned char* s, unsigned int len,
														unsigned short* proto)
{
#define PROTO2UINT(a, b, c) ((	(((unsigned int)(a))<<16)+ \
								(((unsigned int)(b))<<8)+  \
								((unsigned int)(c)) ) | 0x20202020)
	unsigned int i;

	/* must support 3-char arrays for udp, tcp, tls,
	 * must support 4-char arrays for sctp */
	*proto=PROTO_NONE;
	if (len!=3 && len!=4) return -1;

	i=PROTO2UINT(s[0], s[1], s[2]);
	switch(i){
		case PROTO2UINT('u', 'd', 'p'):
			if(len==3) { *proto=PROTO_UDP; return 0; }
			break;
#ifdef USE_TCP
		case PROTO2UINT('t', 'c', 'p'):
			if(len==3) { *proto=PROTO_TCP; return 0; }
			break;
#ifdef USE_TLS
		case PROTO2UINT('t', 'l', 's'):
			if(len==3) { *proto=PROTO_TLS; return 0; }
			break;
#endif
#endif
#ifdef USE_SCTP
		case PROTO2UINT('s', 'c', 't'):
			if(len==4 && (s[3]=='p' || s[3]=='P')) {
				*proto=PROTO_SCTP; return 0;
			}
			break;
#endif

		default:
			return -1;
	}
	return -1;
}


void init_protos(void);

void destroy_protos(void);

int load_proto_lib( unsigned short proto );


/********************** PROTO interface stuff ***************************/

#include "socket.h"
#include "../parser/msg_parser.h"

typedef int (*proto_init)(void);

typedef void (*proto_destroy)(void);

typedef int (*proto_init_listener)(struct socket_info *si);

typedef int (*proto_write)(void *ctx, struct socket_info *src, 
			char *buf, unsigned int len,
			union sockaddr_union*  to, void *extra);

typedef int (*proto_event_handler)(struct socket_info *si);

struct proto_funcs {
	unsigned short      default_port;
	proto_init          init;
	proto_destroy       destroy;
	proto_init_listener init_listener;
	proto_event_handler event_handler;
	proto_write         write_message;
};

struct proto_interface {
	char *name;
	char *version;
	char *compile_flags;
	struct proto_funcs funcs;
};

struct proto_info {
	/* proto as ID */
	unsigned short proto;
	/* listeners on this proto */
	struct socket_info *listeners;

	/* functions for this protocol */
	struct proto_funcs funcs;
};

extern struct proto_info protos[PROTO_MAX];

#define proto_used(_n)   (protos[_n].listeners!=NULL)

#define proto_loaded(_n) (protos[_n].funcs.init_listener!=NULL)


#endif

