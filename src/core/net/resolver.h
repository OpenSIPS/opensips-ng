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


#ifndef _H_CORE_NET_RESOLVER
#define _H_CORE_NET_RESOLVER

#include <netdb.h>

#include "../str.h"
#include "../globals.h"



/* Standard (non SIP) DNS resolver.
 * It is blocking, to be used only during init stage
 */
static inline struct hostent* resolvehost(char* name, int no_ip_test)
{
	static struct hostent* he=0;
#ifdef HAVE_GETIPNODEBYNAME 
	int err;
	static struct hostent* he2=0;
#endif
	struct ip_addr ip;
	str s;

	if (!no_ip_test) {
		s.s = (char*)name;
		s.len = strlen(name);
		/* check if it's an ip address */
		if ( (str2ip(&s, &ip) ==0) || ( str2ip6(&s,&ip) == 0) ){
			/* we are lucky, this is an ip address */
			return ip_addr2he(&s, &ip);
		}
	}

	/* ipv4 */
	he=gethostbyname(name);
	if(he==0 && dns_try_ipv6){
		/*try ipv6*/
	#ifdef HAVE_GETHOSTBYNAME2
		he=gethostbyname2(name, AF_INET6);
	#elif defined HAVE_GETIPNODEBYNAME
		/* on solaris 8 getipnodebyname has a memory leak,
		 * after some time calls to it will fail with err=3
		 * solution: patch your solaris 8 installation */
		if (he2) freehostent(he2);
		he=he2=getipnodebyname(name, AF_INET6, 0, &err);
	#else
		#error neither gethostbyname2 or getipnodebyname present
	#endif
	}
	return he;
}


#endif

