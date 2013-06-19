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

#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/uio.h>  /* writev*/
#include <netdb.h>
#include <stdlib.h> /*exit() */


#include "../utils.h"
#include "../mem/mem.h"
#include "socket.h"
#include "proto.h"
#include "resolver.h"


struct socket_info *registered_listeners = NULL;


/*! \brief
 * parses [proto:]host[:port] where proto= udp|tcp|tls|sctp
 * \return 0 on success and -1 on failure
 */
int parse_listener(char* s, int slen, char** host, int* hlen,
								unsigned short* port, unsigned short* proto)
{
	char* first; /* first ':' occurrence */
	char* second; /* second ':' occurrence */
	char* p;
	int   bracket;
	str   tmp;
	char* end;
	unsigned int tmp_port;

	first=second=0;
	bracket=0;
	end = s + slen;

	/* find the first 2 ':', ignoring possible ipv6 addresses
	 * (substrings between [])
	 */
	for(p=s; p<end ; p++){
		switch(*p){
			case '[':
				bracket++;
				if (bracket>1) goto error_brackets;
				break;
			case ']':
				bracket--;
				if (bracket<0) goto error_brackets;
				break;
			case ':':
				if (bracket==0){
					if (first==0) first=p;
					else if( second==0) second=p;
					else goto error_colons;
				}
				break;
		}
	}
	if (p==s) return -1;
	if (*(p-1)==':') goto error_colons;
	
	if (first==0){ /* no ':' => only host */
		*host=s;
		*hlen=(int)(p-s);
		*port=0;
		*proto=0;
		return 0;
	}
	if (second){ /* 2 ':' found => check if valid */
		if (parse_proto((unsigned char*)s, first-s, proto)<0)
			goto error_proto;
		tmp.s = second+1;
		tmp.len = end - tmp.s;
		if (str2int( &tmp, &tmp_port )==-1) goto error_port;
		*port = tmp_port;
		*host=first+1;
		*hlen=(int)(second-*host);
		return 0;
	}
	/* only 1 ':' found => it's either proto:host or host:port */
	tmp.s = first+1;
	tmp.len = end - tmp.s;
	if (str2int( &tmp, (unsigned int*)port )==-1) {
		/* invalid port => it's proto:host */
		if (parse_proto((unsigned char*)s, first-s, proto)<0) goto error_proto;
		*port=0;
		*host=first+1;
		*hlen=(int)(p-*host);
	}else{
		/* valid port => its host:port */
		*proto=0;
		*host=s;
		*hlen=(int)(first-*host);
	}
	return 0;
error_brackets:
	LM_ERR("too many brackets in %s\n", s);
	return -1;
error_colons:
	LM_ERR(" too many colons in %s\n", s);
	return -1;
error_proto:
	LM_ERR("bad protocol in %s\n", s);
	return -1;
error_port:
	LM_ERR("bad port number in %s\n", s);
	return -1;
}


/*
 *  Returns 0 o success, -1 on error
 */
int register_listener(char *s)
{
	int s_len;
	str host;
	unsigned short port;
	unsigned short proto;
	struct socket_info *si;
	struct socket_info *it;

	if ( s==NULL || (s_len=strlen(s))==0) {
		LM_ERR("NULL or empty listener\n");
		return -1;
	}

	/* first parse the socket to see if valid */
	if (parse_listener( s, s_len, &host.s, &host.len, &port, &proto)!=0) {
		LM_ERR("failed to parse listener <%s>\n",s);
		return -1;
	}

	/* alloc a new socket structure - the socket name (as string) will
	   be allocated in the same memory chunck */
	si = (struct socket_info*)shm_malloc(sizeof(struct socket_info)+s_len+1);
	if (si==NULL) {
		LM_ERR("no more shm memory (required %d)\n",
			(int)(sizeof(struct socket_info)+s_len) );
		return -1;
	}
	memset( si, 0, sizeof(struct socket_info));

	/* socket name (copy as NULL terminated string) */
	si->name.s = (char*)(si+1);
	memcpy( si->name.s , s, s_len+1 );
	si->name.len = s_len;

	/* host part (linked inside socket name) */
	si->host.len = host.len;
	si->host.s = si->name.s + (host.s - s);

	/* socket port and proto (numrical vals) */
	si->proto = proto;
	si->port = port;

	/* link the new listener in the list (preserver the insert order) */
	for( it=registered_listeners ; it && it->next ; it=it->next );
	if (it==NULL)
		registered_listeners = si;
	else
		it->next = si;

	return 0;
}


int fix_all_listeners(void)
{
	struct socket_info *si;
	struct hostent* he;
	char bk;
	char *tmp;
	int len;

	/* init all registered listeners */
	for( si=registered_listeners ; si ; si=si->next ) {
		LM_DBG("Passing through %p to %p\n", si, registered_listeners);

		/* IP address */
		bk = si->host.s[si->host.len];
		si->host.s[si->host.len] = 0;
		he = resolvehost( si->host.s, 0);
		si->host.s[si->host.len] = bk;
		if (he==0){
			LM_ERR("could not resolve %.*s\n", si->host.len, si->host.s);
			goto error;
		}

		/* convert to ip_addr format */
		hostent2ip_addr( &si->address, he, 0);
		free_hostent( he );
		shm_free( he );

		if ((tmp=ip_addr2a(&si->address))==0)
			goto error;
		if (si->address.af == AF_INET6) {
			si->address_str.s=(char*)shm_malloc(strlen(tmp)+1+2);
			if (si->address_str.s==0){
				LM_ERR("out of pkg memory\n");
				goto error;
			}
			si->address_str.s[0] = '[';
			strncpy( si->address_str.s+1 , tmp, strlen(tmp));
			si->address_str.s[1+strlen(tmp)] = ']';
			si->address_str.s[2+strlen(tmp)] = '\0';
			si->address_str.len=strlen(tmp) + 2;
		} else {
			si->address_str.s=(char*)shm_malloc(strlen(tmp)+1);
			if (si->address_str.s==0){
				LM_ERR("out of pkg memory\n");
				goto error;
			}
			strncpy(si->address_str.s, tmp, strlen(tmp)+1);
			si->address_str.len=strlen(tmp);
		}

		/* convert port to str */
		tmp = int2str(si->port, &len);
		if (len>=MAX_PORT_LEN){
			LM_ERR("bad port number: %d\n", si->port);
			goto error;
		}
		si->port_str.s=(char*)shm_malloc(len+1);
		if (si->port_str.s==NULL){
			LM_ERR("out of pkg memory.\n");
			goto error;
		}
		strncpy(si->port_str.s, tmp, len+1);
		si->port_str.len = len;

		/* build and set string encoding for the real socket info */
		tmp = socket2str( si, 0, &si->sock_str.len, 0);
		if (tmp==0) {
			LM_ERR("failed to convert socket to string\n");
			goto error;
		}
		si->sock_str.s=(char*)shm_malloc(si->sock_str.len);
		if (si->sock_str.s==0) {
			LM_ERR("out of pkg memory.\n");
			goto error;
		}
		memcpy(si->sock_str.s, tmp, si->sock_str.len);

		#ifdef USE_MCAST
		/* Check if it is an multicast address and
		 * set the flag if so
		 */
		if (is_mcast(&si->address)) {
			si->flags |= SI_FLAG_IS_MCAST;
		}
		#endif /* USE_MCAST */

		LM_DBG("listener <%s> succesfully fixed\n",si->name.s);
	}

	return 0;
error:
	LM_CRIT("failed to fix listener <%s> \n",si->name.s);
	return -1;
}


/* Discovers all interfaces and created UDP listeners of default port
 */
int auto_register_listeners(void)
{
	/* is something like this really useful ?? -bogdan */
	// TODO
	return -1;
}


/*
 * go through all registered listeners and initialize them one by one
 * if needed, load the required protocol lib.
 */
int init_all_listeners(void)
{
	struct socket_info *si;
	struct socket_info *next;
	struct socket_info *it;

	/* init all registered listeners */
	for( si=registered_listeners ; si ; si=next ) {

		next = si->next;
		/* fix default proto */
		if (si->proto==PROTO_NONE)
			si->proto = PROTO_UDP;

		/* is proto available? if no -> load proto lib / functions */
		if ( !proto_loaded(si->proto) ) {
			if ( load_proto_lib(si->proto)<0 ) {
				LM_ERR("failed to load protocol lib\n");
				return -1;
			}
		}

		/* fix default port */
		if (si->port==0)
			si->port = protos[si->proto].funcs.default_port;

		/* init the listener socket */
		if ( protos[si->proto].funcs.init_listener( si ) < 0  ) {
			LM_ERR("failed to init listener %.*s\n",si->name.len,si->name.s);
			return -1;
		}

		/* move the listener to corresponding proto list */
		for( it=protos[si->proto].listeners ; it && it->next ; it=it->next );
		if (it==NULL)
			protos[si->proto].listeners = si;
		else
			it->next = si;
		si->next = NULL;

	}

	return 0;
}


void destroy_all_listeners(void)
{
	unsigned short proto;
	struct socket_info *si;
	struct socket_info *next;

	/* destroy all registered listeners */
	for( proto=PROTO_UDP ; proto<PROTO_MAX ; proto++) {
		for( si=protos[proto].listeners ; si ; si=next ) {
			next = si->next;

			/* destroy listener */
			if (si->port_str.s)
				shm_free(si->port_str.s);
			if (si->address_str.s)
				shm_free(si->address_str.s);
			if (si->sock_str.s)
				shm_free(si->sock_str.s);
			shm_free(si);
		}
	}
}

