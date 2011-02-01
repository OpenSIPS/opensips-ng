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

#ifndef _H_NET_SOCKET
#define _H_NET_SOCKET

#include "../str.h"
#include "../utils.h"
#include "ip_addr.h"

#define SI_FLAG_IS_LO    (1<<0)
#define SI_FLAG_IS_MCAST (1<<1)

#define MAX_PORT_LEN 7 /*!< ':' + max 5 letters + \\0 */

struct socket_info {
	str name; /*!< name - eg.: foo.bar or 10.0.0.1 , allocated in the same
	               mem chunk at the structure, NULL terminated */
	str host;
	unsigned short port;  /*!< port number */
	unsigned short proto; /*!< tcp or udp*/
	int socket;

	struct ip_addr address; /*!< ip address */
	str address_str;  /*!< ip address converted to string -- optimization*/
	str port_str;     /*!< port number converted to string -- optimization*/
	unsigned int flags; /*!< SI_IS_IP | SI_IS_LO | SI_IS_MCAST */
	union sockaddr_union su;
	str sock_str;
	//str adv_sock_str;
	//str adv_name_str; /* Advertised name of this interface */
	//str adv_port_str; /* Advertised port of this interface */
	//struct ip_addr adv_address; /* Advertised address in ip_addr form (for find_si) */
	//unsigned short adv_port;    /* optimization for grep_sock_info() */
	struct socket_info* next;
};


struct receive_info {
	struct ip_addr src_ip;
	struct ip_addr dst_ip;
	unsigned short src_port; /*!< host byte order */
	unsigned short dst_port; /*!< host byte order */
	int proto;
	int proto_reserved1; /*!< tcp stores the connection id here */
	int proto_reserved2;
	union sockaddr_union src_su; /*!< useful for replies*/
	struct socket_info* bind_address; /*!< sock_info structure on which the msg was received*/
};


#include "proto.h"

extern struct socket_info *registered_listeners;

#define MAX_SOCKET_STR ( 4 + 1 + IP_ADDR_MAX_STR_SIZE+1+INT2STR_MAX_LEN+1)
#define sock_str_len(_sock) ( 3 + 1*((_sock)->proto==PROTO_SCTP) + 1 + \
		(_sock)->address_str.len + 1 + (_sock)->port_str.len)

static inline char* socket2str(struct socket_info *sock, char *s, int* len, int adv)
{
	static char buf[MAX_SOCKET_STR];
	char *p,*p1;

	if (s) {
		/* buffer provided -> check lenght */
		if ( sock_str_len(sock) > *len ) {
			LM_ERR("buffer too short\n");
			return 0;
		}
		p = p1 = s;
	} else {
		p = p1 = buf;
	}

	p = proto2str( sock->proto, p);
	if (p==NULL) return 0;

	*(p++) = ':';
	#if 0
	FIXME
	if(adv) {
		memcpy( p, sock->adv_name_str.s, sock->adv_name_str.len);
		p += sock->adv_name_str.len;
		*(p++) = ':';
		memcpy( p, sock->adv_port_str.s, sock->adv_port_str.len);
		p += sock->adv_port_str.len;
	} else {
	#endif
	memcpy( p, sock->address_str.s, sock->address_str.len);
	p += sock->address_str.len;
	*(p++) = ':';
	memcpy( p, sock->port_str.s, sock->port_str.len);
	p += sock->port_str.len;

	*len = (int)(long)(p-p1);
	LM_DBG("<%.*s>\n",*len,p1);
	return p1;
}



/* Parses and breaks the listener definition into pars 
 */
int parse_listener(char* s, int slen, char** host, int* hlen,
		unsigned short* port, unsigned short* proto);


/* Registers a new network listener for opensips
 * The socket is validated as format, copied, but not initalized.
 */
int register_listener(char *s);


/* Discovers all interfaces and created UDP listeners of default port
 */
int auto_register_listeners(void);


/* Fixes (ip resolving, optimizations) all registered listeners
 */
int fix_all_listeners(void);

/* Initalizes all registered listeners
 */
int init_all_listeners(void);


/* Destroys all registered listeners
 */
void destroy_all_listeners(void);

#endif

