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
 *  2010-07-xx  created (adragus)
 */
#ifndef __resolve_h
#define __resolve_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/nameser.h>

#ifdef __OS_darwin
#include <arpa/nameser_compat.h>
#endif

#include <ares.h>
#include "../globals.h"
#include "../utils.h"
#include "../net/ip_addr.h"
#include "../net/proto.h"
#include "parsers.h"
#include "../config.h"

/* Data structure used for fail-over of DNS queries.
 * It represents a linked list of queries which can still be attempted.
 * It should not be accessed directly by the user, but only with next_he.
 */
typedef struct _dns_request
{
	int type;
	char name[MAX_DNS_NAME];
	unsigned short name_len;

	unsigned short is_sips;
	unsigned short port;
	unsigned short proto;

	struct _dns_request *next;

} dns_request_t;



typedef  void (dns_resolve_answer ) (void * , struct hostent* );
typedef  void (dns_get_record_answer ) (void * , struct rdata* );
typedef  void (dns_sip_resolve_answer ) (void * arg, unsigned short port,
								unsigned short proto, struct hostent* he,
								dns_request_t * requests);

int resolv_init(void);

void get_record(char* name, int type, dns_get_record_answer func, void * arg);

void resolvehost(char * name, dns_resolve_answer func, void * arg);

void sip_resolvehost(str* name, unsigned short port,	unsigned short proto,
		int is_sips, dns_sip_resolve_answer func, void * arg);

void next_he(dns_request_t * requests, dns_sip_resolve_answer func, void * arg );

void free_request_list(dns_request_t *);


#endif
