/*
 * $Id: msg_parser.h 6495 2010-01-04 15:46:17Z bogdan_iancu $
 *
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


#ifndef MSG_PARSER_H
#define MSG_PARSER_H

#include <strings.h>
#include "../net/socket.h"
#include "../str.h"
#include "../net/ip_addr.h"
#include "defines.h"
#include "parse_def.h"
#include "parse_cseq.h"
#include "parse_via.h"
#include "parse_fline.h"
#include "parse_multipart.h"
#include "hf.h"
#include "sdp/sdp.h"


/* convenience short-cut macros */
#define REQ_LINE(_msg) ((_msg)->first_line.u.request)
#define REQ_METHOD first_line.u.request.method_value
#define REPLY_STATUS first_line.u.reply.statuscode
#define REPLY_CLASS(_reply) ((_reply)->REPLY_STATUS/100)

/* number methods as power of two to allow bitmap matching */
enum request_method {
	METHOD_UNDEF=0,           /* 0 - --- */
	METHOD_INVITE=1,          /* 1 - 2^0 */
	METHOD_CANCEL=2,          /* 2 - 2^1 */
	METHOD_ACK=4,             /* 3 - 2^2 */
	METHOD_BYE=8,             /* 4 - 2^3 */
	METHOD_INFO=16,           /* 5 - 2^4 */
	METHOD_OPTIONS=32,        /* 6 - 2^5 */
	METHOD_UPDATE=64,         /* 7 - 2^6 */
	METHOD_REGISTER=128,      /* 8 - 2^7 */
	METHOD_MESSAGE=256,       /* 9 - 2^8 */
	METHOD_SUBSCRIBE=512,     /* 10 - 2^9 */
	METHOD_NOTIFY=1024,       /* 11 - 2^10 */
	METHOD_PRACK=2048,        /* 12 - 2^11 */
	METHOD_REFER=4096,        /* 13 - 2^12 */
	METHOD_PUBLISH=8192,      /* 14 - 2^13 */
	METHOD_OTHER=16384        /* 15 - 2^14 */
};

#define FL_FORCE_RPORT       (1<<0)  /* force rport (top via) */
#define FL_FORCE_ACTIVE      (1<<1)  /* force active SDP */
#define FL_FORCE_LOCAL_RPORT (1<<2)  /* force local rport (local via) */
#define FL_SDP_IP_AFS        (1<<3)  /* SDP IP rewritten */
#define FL_SDP_PORT_AFS      (1<<4)  /* SDP port rewritten */
#define FL_SHM_CLONE         (1<<5)  /* msg cloned in SHM as a single chunk */
#define FL_USE_UAC_FROM      (1<<6)  /* take FROM hdr from UAC insteas of UAS*/
#define FL_USE_UAC_TO        (1<<7)  /* take TO hdr from UAC insteas of UAS */
#define FL_USE_UAC_CSEQ      (1<<8)  /* take CSEQ hdr from UAC insteas of UAS*/
#define FL_REQ_UPSTREAM      (1<<9)  /* it's an upstream going request */
#define FL_DO_KEEPALIVE      (1<<10) /* keepalive request's source after a 
                                      * positive reply */
#define FL_USE_MEDIA_PROXY   (1<<11) /* use mediaproxy on all messages during
                                      * a dialog */


#define IFISMETHOD(methodname,firstchar)                                  \
if (  (*tmp==(firstchar) || *tmp==((firstchar) | 32)) &&                  \
        strncasecmp( tmp+1, #methodname +1, methodname##_LEN-1)==0 &&     \
        *(tmp+methodname##_LEN)==' ') {                                   \
                fl->type=SIP_REQUEST;                                     \
                fl->u.request.method.len=methodname##_LEN;                \
                fl->u.request.method_value=METHOD_##methodname;           \
                tmp=buffer+methodname##_LEN;                              \
}


/*
 * Return a URI to which the message should be really sent (not what should
 * be in the Request URI. The following fields are tried in this order:
 * 1) dst_uri
 * 2) new_uri
 * 3) first_line.u.request.uri
 */
#define GET_NEXT_HOP(m) \
(((m)->dst_uri.s && (m)->dst_uri.len) ? (&(m)->dst_uri) : \
(((m)->new_uri.s && (m)->new_uri.len) ? (&(m)->new_uri) : (&(m)->first_line.u.request.uri)))


/*
 * Return the Reqeust URI of a message.
 * The following fields are tried in this order:
 * 1) new_uri
 * 2) first_line.u.request.uri
 */
#define GET_RURI(m) \
(((m)->new_uri.s && (m)->new_uri.len) ? (&(m)->new_uri) : (&(m)->first_line.u.request.uri))


enum _uri_type{ERROR_URI_T=0, SIP_URI_T, SIPS_URI_T, TEL_URI_T, TELS_URI_T};
typedef enum _uri_type uri_type;

struct sip_uri {
	str user;     /* Username */
	str passwd;   /* Password */
	str host;     /* Host name */
	str port;     /* Port number */
	str params;   /* Parameters */
	str headers;  
	unsigned short port_no;
	unsigned short proto; /* from transport */
	uri_type type; /* uri scheme */
	/* parameters */
	str transport;
	str ttl;
	str user_param;
	str maddr;
	str method;
	str lr;
	str r2; /* ser specific rr parameter */
	/* values */
	str transport_val;
	str ttl_val;
	str user_param_val;
	str maddr_val;
	str method_val;
	str lr_val; /* lr value placeholder for lr=on a.s.o*/
	str r2_val;
};


#include "parse_to.h"

struct sip_msg {
	unsigned int id;               /* message id, unique/process*/
	struct msg_start first_line;   /* Message first line */
	struct via_body* via1;         /* The first via */
	struct via_body* via2;         /* The second via */
	struct hdr_field* headers;     /* All the parsed headers*/
	struct hdr_field* last_header; /* Pointer to the last parsed header*/
	hdr_flags_t parsed_flag;       /* Already parsed header field types */

	/* Via, To, CSeq, Call-Id, From, end of header*/
	/* pointers to the first occurrences of these headers;
	 * everything is also saved in 'headers'
	 * (WARNING: do not deallocate them twice!)*/

	struct hdr_field* h_via;
	struct hdr_field* callid;
	struct hdr_field* to;
	struct hdr_field* cseq;
	struct hdr_field* from;
	struct hdr_field* contact;
	struct hdr_field* maxforwards;
	struct hdr_field* route;
	struct hdr_field* record_route;
	struct hdr_field* path;
	struct hdr_field* content_type;
	struct hdr_field* content_length;
	struct hdr_field* authorization;
	struct hdr_field* expires;
	struct hdr_field* proxy_auth;
	struct hdr_field* supported;
	struct hdr_field* proxy_require;
	struct hdr_field* unsupported;
	struct hdr_field* allow;
	struct hdr_field* event;
	struct hdr_field* accept;
	struct hdr_field* accept_language;
	struct hdr_field* organization;
	struct hdr_field* priority;
	struct hdr_field* subject;
	struct hdr_field* user_agent;
	struct hdr_field* content_disposition;
	struct hdr_field* accept_disposition;
	struct hdr_field* diversion;
	struct hdr_field* rpid;
	struct hdr_field* refer_to;
	struct hdr_field* session_expires;
	struct hdr_field* min_se;
	struct hdr_field* ppi;
	struct hdr_field* pai;
	struct hdr_field* privacy;

	struct sdp_info* sdp;

	struct multi_body * multi;

	char* eoh;        /* pointer to the end of header (if found) or null */
	char* unparsed;   /* here we stopped parsing*/
	
	struct receive_info rcv; /* source & dest ip, ports, proto a.s.o*/

	char* buf;        /* scratch pad, holds a unmodified message,
                           *  via, etc. point into it */
	unsigned int len; /* message len (orig) */
	unsigned int new_len; /* message len after header modification */

	/* modifications */

	str new_uri; /* changed first line uri, when you change this
                  * don't forget to set parsed_uri_ok to 0 */

	str dst_uri; /* Destination URI, must be forwarded to this URI if len!=0 */
	
	/* current uri */
	int parsed_uri_ok; /* 1 if parsed_uri is valid, 0 if not, set it to 0
	                      if you modify the uri (e.g change new_uri)*/
	struct sip_uri parsed_uri; /* speed-up > keep here the parsed uri*/

	/* the same for original uri */
	int parsed_orig_ruri_ok;
	struct sip_uri parsed_orig_ruri;

	/* whatever whoever want to append to branch comes here */
	//char add_to_branch_s[MAX_BRANCH_PARAM_LEN];
	//int add_to_branch_len;
	
	/* index to TM hash table; stored in core to avoid 
	 * unnecessary calculations */
	//unsigned int  hash_index;

	/* flags used from script */
	//TODO add flags
	//flag_t flags;

	/* flags used by core - allows to set various flags on the message; may 
	 * be used for simple inter-module communication or remembering 
	 * processing state reached */
	//unsigned int msg_flags;

	//str set_global_address;
	//str set_global_port;

	/* force sending on this socket */
	struct socket_info* force_send_socket;

	/* create a route HF out of this path vector */
	str path_vec;
};


/* pointer to a fakes message which was never received ;
   (when this message is "relayed", it is generated out
    of the original request)
*/
#define FAKED_REPLY     ((struct sip_msg *) -1)

int parse_msg(struct sip_msg* msg, hdr_flags_t flags);

int parse_headers(struct sip_msg* msg, hdr_flags_t flags, int next);

struct multi_body * get_all_bodies(struct sip_msg * );

char* get_hdr_field(char* buf, char* end, struct hdr_field* hdr);

void free_sip_msg(struct sip_msg* msg);

/* make sure all HFs needed for transaction identification have been
   parsed; return 0 if those HFs can't be found
 */

int check_transaction_quadruple( struct sip_msg* msg );


/* returns a pointer to the begining of the msg's body
 */
inline static char* get_body(struct sip_msg *msg)
{
	int offset;
	unsigned int len;

	if ( parse_headers(msg,HDR_EOH_F, 0)==-1 )
		return 0;

	if (msg->unparsed){
		len=(unsigned int)(msg->unparsed-msg->buf);
	}else return 0;
	if ((len+2<=msg->len) && (strncmp(CRLF,msg->unparsed,CRLF_LEN)==0) )
		offset = CRLF_LEN;
	else if ( (len+1<=msg->len) &&
				(*(msg->unparsed)=='\n' || *(msg->unparsed)=='\r' ) )
		offset = 1;
	else
		return 0;

	return msg->unparsed + offset;
}


/* 
 * Search through already parsed headers (no parsing done) a non-standard
 * header - all known headers are skipped! 
 */
#define get_header_by_static_name(_msg, _name) \
		get_header_by_name(_msg, _name, sizeof(_name)-1)
inline static struct hdr_field *get_header_by_name( struct sip_msg *msg,
													char *s, unsigned int len)
{
	struct hdr_field *hdr;

	for( hdr=msg->headers ; hdr ; hdr=hdr->next ) {
		if (hdr->type==HDR_OTHER_T && len==hdr->name.len
		&& strncasecmp(hdr->name.s,s,len)==0)
			return hdr;
	}
	return NULL;
}


/*
 * Make a private copy of the string and assign it to new_uri (new RURI)
 */
int set_ruri(struct sip_msg* msg, str* uri);


/*
 * Make a private copy of the string and assign it to dst_uri
 */
int set_dst_uri(struct sip_msg* msg, str* uri);

/*
 * Make a private copy of the string and assign it to path_vec
 */
int set_path_vector(struct sip_msg* msg, str* path);


#endif
