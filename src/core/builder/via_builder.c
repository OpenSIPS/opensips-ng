/*
 *
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
 *  2010-12-xx  created (bogdan)
 */


#include "../config.h"
#include "via_builder.h"


#define VIA_HDR_UDP  "SIP/2.0/UDP "
#define VIA_HDR_UDP_LEN (sizeof(VIA_HDR_UDP) - 1)
#define VIA_HDR_TCP  "SIP/2.0/TCP "
#define VIA_HDR_TCP_LEN (sizeof(VIA_HDR_TCP) - 1)
#define VIA_HDR_TLS  "SIP/2.0/TLS "
#define VIA_HDR_TLS_LEN (sizeof(VIA_HDR_TLS) - 1)
#define VIA_HDR_SCTP "SIP/2.0/SCTP "
#define VIA_HDR_SCTP_LEN (sizeof(VIA_HDR_SCTP) - 1)

#define VIA_BRANCH_PARAM ";branch="
#define VIA_BRANCH_PARAM_LEN (sizeof(VIA_BRANCH_PARAM) - 1)





static inline str* get_sl_branch(struct sip_msg* msg)
{
	static str default_branch = str_init("0");
	struct hdr_field *h_via;
	struct via_body  *b_via;
	str *branch;
	int via_parsed;

	via_parsed = 0;
	branch = 0;

	/* first VIA header must be parsed */
	for( h_via=msg->h_via ; h_via ; h_via=h_via->next_sibling ) {

		b_via = (struct via_body*)h_via->parsed;
		for( ; b_via ; b_via=b_via->next ) {
			/* check if there is any valid branch param */
			if (b_via->branch==0 || b_via->branch->value.s==0
			|| b_via->branch->value.len==0 )
				continue;
			branch = &b_via->branch->value;
			/* check if the branch param has the magic cookie */
			if (branch->len <= (int)MCOOKIE_LEN
			|| memcmp( branch->s, MCOOKIE, MCOOKIE_LEN)!=0 )
				continue;
			/* found a statefull branch -> use it */
			goto found;
		}

		if (!via_parsed) {
			if ( parse_headers(msg,HDR_EOH_F,0)<0 ) {
				LM_ERR("failed to parse all hdrs\n");
				return 0;
			}
			via_parsed = 1;
		}
	}

	/* no statefull branch :(.. -> use the branch from the last via */
found:
	if (branch==NULL) {
		/* no branch found/...use a default one*/
		branch = &default_branch;
	}
	return branch;
}



int via_builder( struct sip_msg *msg, struct socket_info* send_sock, str *via)
{
	char *p;
	str *branch;

	/* get branch in stateless mode */
	branch = get_sl_branch(msg);
	if (branch==NULL) {
		LM_ERR("failed to get sl branch\n");
		return -1;
	}

	/* calculate the len */
	via->len= VIA_HDR_UDP_LEN+1 + send_sock->address_str.len + 2 /*if ipv6*/
		+ 1 /*':'*/ + send_sock->port_str.len
		+ VIA_BRANCH_PARAM_LEN + branch->len + CRLF_LEN;

	via->s = shm_malloc( via->len );
	if (via->s==NULL) {
		LM_ERR("out of shm memory\n");
		return -1;
	}

	/* build the VIA buffer */
	p = via->s;
	if (send_sock->proto==PROTO_UDP){
		memcpy( p, VIA_HDR_UDP, VIA_HDR_UDP_LEN);
		p += VIA_HDR_UDP_LEN;
	}else if (send_sock->proto==PROTO_TCP){
		memcpy( p, VIA_HDR_TCP, VIA_HDR_TCP_LEN);
		p += VIA_HDR_TCP_LEN;
	}else if (send_sock->proto==PROTO_TLS){
		memcpy( p, VIA_HDR_TLS, VIA_HDR_TLS_LEN);
		p += VIA_HDR_TLS_LEN;
	}else if(send_sock->proto==PROTO_SCTP){
		memcpy( p, VIA_HDR_SCTP, VIA_HDR_SCTP_LEN);
		p += VIA_HDR_SCTP_LEN;
	}else{
		LM_CRIT("unknown proto %d\n", send_sock->proto);
		return -1;
	}

	memcpy( p, send_sock->address_str.s, send_sock->address_str.len);
	p += send_sock->address_str.len;

	*(p++)=':';
	memcpy( p, send_sock->port_str.s, send_sock->port_str.len);
	p += send_sock->port_str.len;

	/* branch parameter */
	memcpy( p, VIA_BRANCH_PARAM, VIA_BRANCH_PARAM_LEN );
	p += VIA_BRANCH_PARAM_LEN;
	memcpy( p, branch->s, branch->len );
	p += branch->len;

	memcpy( p, CRLF, CRLF_LEN);
	p += CRLF_LEN;
	via->len = p - via->s;
	return 0;
}


