/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2010 OpenSIPS Project
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


#ifndef _DNS_PARSERS
#define _DNS_PARSERS

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <stdlib.h>


#define MAX_QUERY_SIZE 8192
#define ANS_SIZE       8192
#define DNS_HDR_SIZE     12
#define MAX_DNS_NAME 256
#define MAX_DNS_STRING 255

/*! \brief this is not official yet */
#define T_EBL		65300

/*! \brief query union*/
union dns_query{
	HEADER hdr;
	unsigned char buff[MAX_QUERY_SIZE];
};


/*! \brief rdata struct*/
struct rdata {
	unsigned short type;
	unsigned short class;
	unsigned int   ttl;
	void* rdata;
	struct rdata* next;
};


/*! \brief srv rec. struct*/
struct srv_rdata {
	unsigned short priority;
	unsigned short weight;
	unsigned short running_sum;
	unsigned short port;
	unsigned int name_len;
	char name[MAX_DNS_NAME];
};

/*! \brief naptr rec. struct*/
struct naptr_rdata {
	unsigned short order;
	unsigned short pref;
	unsigned int flags_len;
	char flags[MAX_DNS_STRING];
	unsigned int services_len;
	char services[MAX_DNS_STRING];
	unsigned int regexp_len;
	char regexp[MAX_DNS_STRING];
	unsigned int repl_len; /* not currently used */
	char repl[MAX_DNS_NAME];
};


/*! \brief A rec. struct */
struct a_rdata {
	unsigned char ip[4];
};

struct aaaa_rdata {
	unsigned char ip6[16];
};

/*! \brief cname rec. struct*/
struct cname_rdata {
	char name[MAX_DNS_NAME];
};

/*! \brief txt rec. struct
\note	This is not strictly correct as TXT records *could* contain multiple strings. */
struct txt_rdata {
	char txt[MAX_DNS_NAME];
};

/*! \brief EBL rec. struct
\note This is an experimental RR for infrastructure ENUM */
struct ebl_rdata {
	unsigned char position;
	unsigned int separator_len;
	char separator[MAX_DNS_NAME];
	unsigned int apex_len;
	char apex[MAX_DNS_NAME];
};


#define get_naptr(_rdata) \
	( ((struct naptr_rdata*)(_rdata)->rdata) )

#define get_srv(_rdata) \
	( ((struct srv_rdata*)(_rdata)->rdata) )

#define HEX2I(c) \
	(	(((c)>='0') && ((c)<='9'))? (c)-'0' :  \
		(((c)>='A') && ((c)<='F'))? ((c)-'A')+10 : \
		(((c)>='a') && ((c)<='f'))? ((c)-'a')+10 : -1 )



struct srv_rdata* dns_srv_parser(unsigned char* msg, unsigned char* end,
								 unsigned char* rdata);

struct naptr_rdata* dns_naptr_parser(unsigned char* msg, unsigned char* end,
									 unsigned char* rdata);

struct cname_rdata* dns_cname_parser(unsigned char* msg, unsigned char* end,
									 unsigned char* rdata);

struct a_rdata* dns_a_parser(unsigned char* rdata, unsigned char* end);

struct aaaa_rdata* dns_aaaa_parser(unsigned char* rdata, unsigned char* end);

struct txt_rdata* dns_txt_parser(unsigned char* msg, unsigned char* end,
								 unsigned char* rdata);

struct ebl_rdata* dns_ebl_parser(unsigned char* msg, unsigned char* end,
								 unsigned char* rdata);

struct rdata* parse_record(unsigned char* answer, int size);

int get_naptr_proto(struct naptr_rdata *n);

void filter_and_sort_naptr(struct rdata** head_p,
		struct rdata** filtered_p, int is_sips);

void sort_srvs(struct rdata **head);

void free_rdata_list(struct rdata* head);


#endif

