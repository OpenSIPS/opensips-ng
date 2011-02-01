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
#include "resolve.h"
#include "dns_globals.h"

/*
 * Types of nodes in the dns_request list
 *
 */
enum
{
	DNS_A_REQUEST = ns_t_a,
	DNS_AAAA_REQUEST = ns_t_aaaa,
	DNS_SRV_REQUEST = ns_t_srv,
	DNS_NAPTR_REQUEST = ns_t_naptr
};

/*
 * Structure that wraps the information in the queries while
 * it goes through the dispatcher
 *
 */
typedef struct _sip_pack
{
	/* fields given by the user */
	dns_sip_resolve_answer * func;	/*user callback function */
	void * arg;						/* user callback argument */

	/* fields representing the answer to the query*/
	unsigned short port;
	unsigned short proto;
	struct hostent * he;
	dns_request_t * requests;

} sip_pack_t;


void print_requests(dns_request_t * l, int status);
int dns_type[] = {DNS_A_REQUEST, DNS_AAAA_REQUEST};

/*
 * Function that will take a pack, unwrap it and call the user callback.
 * This function will be called by the thread that is waiting on the dispatcher.
 */
int sip_dns_to_user(void * arg)
{
	sip_pack_t * r = (sip_pack_t *) arg;
	r->func(r->arg, r->port, r->proto, r->he, r->requests);
	shm_free(arg);

	return 0;
}


int natpr_2_srv(dns_request_t * req)
{
	/* check len */
	if ((req->name_len+SRV_MAX_PREFIX_LEN+1)>MAX_DNS_NAME) {
		LM_WARN("domain name too long (%d),"
			" unable to perform SRV lookup\n", req->name_len);
		return -1;
	}

	switch (req->proto) {
	case PROTO_UDP:
		memmove(req->name+SRV_UDP_PREFIX_LEN,req->name,req->name_len+1);
		memcpy(req->name, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
		req->name_len += SRV_UDP_PREFIX_LEN;
		break;
	case PROTO_TCP:
		if (tcp_disable) goto error;
		memmove(req->name+SRV_TCP_PREFIX_LEN,req->name,req->name_len+1);
		memcpy(req->name, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
		req->name_len += SRV_TCP_PREFIX_LEN;
		break;
	case PROTO_TLS:
		if (tls_disable) goto error;
		memmove(req->name+SRV_TLS_PREFIX_LEN,req->name,req->name_len+1);
		memcpy(req->name, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
		req->name_len += SRV_TLS_PREFIX_LEN;
		break;
#ifdef USE_SCTP
	case PROTO_SCTP:
		if (sctp_disable) goto error;
		memmove(req->name+SRV_SCTP_PREFIX_LEN,req->name,req->name_len+1);
		memcpy(req->name, SRV_SCTP_PREFIX, SRV_SCTP_PREFIX_LEN);
		req->name_len += SRV_SCTP_PREFIX_LEN;
		break;
#endif
	default:
		goto error;
	}

	req->type = DNS_SRV_REQUEST;
	return 0;
error:
	LM_ERR("unsupported proto %d\n",req->proto);
	return -1;
}


int srv_2_a(dns_request_t * req)
{
	if (req->type == DNS_SRV_REQUEST) {
		switch (req->proto) {
		case PROTO_UDP:
			req->name_len -= SRV_UDP_PREFIX_LEN;
			memmove(req->name, req->name+SRV_UDP_PREFIX_LEN, req->name_len+1);
			break;
		case PROTO_TCP:
			req->name_len -= SRV_TCP_PREFIX_LEN;
			memmove(req->name, req->name+SRV_TCP_PREFIX_LEN, req->name_len+1);
			break;
		case PROTO_TLS:
			req->name_len -= SRV_TLS_PREFIX_LEN;
			memmove(req->name, req->name+SRV_TLS_PREFIX_LEN, req->name_len+1);
			break;
#ifdef USE_SCTP
		case PROTO_SCTP:
			req->name_len -= SRV_SCTP_PREFIX_LEN;
			memmove(req->name, req->name+SRV_SCTP_PREFIX_LEN, req->name_len+1);
			break;
#endif
		}
	}
	req->type = DNS_A_REQUEST;
	return 0;
}


/*
 * Callback that will be called by the ares library.
 *
 */

void sip_ares_to_dns(void *arg, int status, int timeouts,
					 unsigned char *abuf, int alen)
{
	sip_pack_t * pack = (sip_pack_t *) arg;
	dns_request_t * req = pack->requests;
	dns_request_t ** last, * new;
	struct srv_rdata *srv;
	struct rdata *head;
	struct rdata *rd;
	struct hostent * tmp;
	int ret,i, reused;

	pack->port = req->port;
	pack->proto = req->proto;
	reused = 0;

	print_requests(pack->requests,status);

	if (status == ARES_SUCCESS)
	{
		/* take the current dns request and based on its type
		 * return the result to the user using the dispatcher or
		 * attempt another dns query
		 */
		switch (req->type)
		{
		case DNS_A_REQUEST:
		case DNS_AAAA_REQUEST:

			if( req->type == DNS_A_REQUEST )
				ret = ares_parse_a_reply(abuf, alen, &tmp, NULL, NULL);
			else
				ret = ares_parse_aaaa_reply(abuf, alen, &tmp, NULL, NULL);

			if (ret != ARES_SUCCESS)
			{
				LM_ERR("Error parsing reply: %s\n", ares_strerror(status));
				goto fail_over;
			} 
	
			pack->requests = req->next;
			pack->he = hostent_cpy(tmp);
			ares_free_hostent(tmp);

			goto submit;

		case DNS_SRV_REQUEST:

			head = parse_record(abuf, alen);

			if (!head)
				goto fail_over;
			sort_srvs(&head);

			pack->requests = req->next;
			last = &pack->requests;

			for (rd = head; rd; rd = rd->next)
			{
				srv = (struct srv_rdata*) rd->rdata;

				if (srv == 0)
				{
					LM_CRIT("null rdata\n");
					free_rdata_list(head);
					goto fail_over;
				}

				for( i =0; i< (dns_try_ipv6?2:1); i++ )
				{
					/* build the new dns requests coresponding
					 * to the information in the SRV reply
					 * Add ipv6 only if it is enabled
					 */
					
					new = shm_malloc(sizeof (*new));

					if( new == NULL )
					{
						LM_ERR("Out of memory\n");
						goto error;
					}

					new->port = srv->port;
					new->proto = pack->proto;
					new->is_sips = req->is_sips;

					new->type = dns_type[i];
					memcpy(new->name, srv->name, srv->name_len);
					new->name_len = srv->name_len;

					new->next = *last;
					*last = new;
					last = &new->next;
				}
			}

			if (head)
				free_rdata_list(head);
			goto search_again;

		case DNS_NAPTR_REQUEST:

			head = parse_record(abuf, alen);

			if (!head)
				goto fail_over;

			filter_and_sort_naptr(&head, &rd, req->is_sips);
			/* free what is useless */
			free_rdata_list(rd);

			/* process the NAPTR records */
			pack->requests = req->next;
			last = &pack->requests;

			for (rd = head; rd; rd = rd->next)
			{
				/* build the new SRV requests based on the NAPTR reply */
				new = shm_malloc(sizeof (*new));

				if( new == NULL )
				{
					LM_ERR("Out of memory\n");
					goto error;
				}

				new->proto = get_naptr_proto(get_naptr(rd));
				new->is_sips = req->is_sips;

				new->type = DNS_SRV_REQUEST;
				memcpy(new->name, get_naptr(rd)->repl,get_naptr(rd)->repl_len);
				new->name_len = get_naptr(rd)->repl_len;

				new->next = *last;
				*last = new;
				last = &new->next;

			}

			if (head)
				free_rdata_list(head);

			goto search_again;

		}

	} else if (status==ARES_ENODATA || status==ARES_ENOTFOUND) {
		switch (req->type)
		{
		case DNS_A_REQUEST:
		case DNS_AAAA_REQUEST:
			/* try next available record */
			goto fail_over;
		case DNS_NAPTR_REQUEST:
			/* fall back to do direct SRV */
			/* NAPTR req can be only one (as root), so we can re-shape it
			   as a SRV req to avoid extra ops */
			LM_DBG("no valid NAPTR record found for %s," 
				" trying direct SRV lookup...\n", req->name);
			/* set default proto */
			req->proto = (req->is_sips)?PROTO_TLS:PROTO_UDP;
			if ( natpr_2_srv( req ) == 0) {
				reused = 1;
				goto search_again;
			}
			/* is conversion failed, fallback to direct A lookup */
		case DNS_SRV_REQUEST:
			/* fall back to do direct A lookup */
			/* set default port */
			req->port = (req->is_sips||((req->proto)==PROTO_TLS))?
				SIPS_PORT:SIP_PORT;
			srv_2_a( req );
			reused = 1;
			goto search_again;
		}

	} else
	{
		LM_ERR("Error in dns reply: %s\n", ares_strerror(status));
		goto fail_over;

	}
	goto free_request;

fail_over:
	/* skip current request an try the next one*/
	pack->requests = req->next;

search_again:
	/* search again for the new request */
	if (pack->requests != NULL)
	{
		LM_DBG("Submiting query for %s type %d\n",
			   pack->requests->name, pack->requests->type);
		ares_search(channel, pack->requests->name, ns_c_in,
					pack->requests->type, sip_ares_to_dns, pack);
	}else
	{
		goto error;
	}

	goto free_request;

error:
	/* set the answer to error and return it to the user */
	pack->he = NULL;

submit:
	/* return the answer to the user using the dispatcher */
	put_task_simple(reactor_in->disp, TASK_PRIO_RESUME_EXEC,
					sip_dns_to_user, pack);

free_request:
	/* free current request */
	if (!reused) shm_free(req);
}


void sip_resolvehost(str* name, unsigned short port, unsigned short proto,
					int is_sips, dns_sip_resolve_answer func, void * arg)
{
	struct ip_addr ip;
	dns_request_t* req;
	struct hostent * he;

	/* check if it's an ip address */
	if ( str2ip(name,&ip) == 0  || ( str2ip6(name,&ip) == 0) )
	{
		/* we are lucky, this is an ip address */
		if ( proto == PROTO_NONE )
			proto = (is_sips) ? PROTO_TLS : PROTO_UDP;
		if ( port == 0 )
			port = (is_sips || (proto == PROTO_TLS)) ? SIPS_PORT : SIP_PORT;

		he = ip_addr2he(name,&ip);

		func(arg, port, proto, he, NULL);
		return;
	}

	req = (dns_request_t *) shm_malloc(sizeof(dns_request_t));
	if( req == NULL)
	{
		LM_ERR("Out of memory\n");
		goto error;
	}
	req->is_sips = is_sips;
	req->port = port;
	req->proto = proto;
	req->next = 0;

	/* do we have a port? */
	if (port != 0)
	{
		/* have port -> no NAPTR, no SRV lookup, just A record lookup */
		LM_DBG("has port -> do A record lookup!\n");
		/* set default PROTO if not set */
		if (proto == PROTO_NONE)
			req->proto = (is_sips) ? PROTO_TLS : PROTO_UDP;
		goto do_a;
	}


	/* no port... what about proto? */
	if (proto != PROTO_NONE)
	{
		/* have proto, but no port -> do SRV lookup */
		LM_DBG("no port, has proto -> do SRV lookup!\n");
		if (is_sips && proto == PROTO_TLS)
		{
			LM_ERR("forced proto %d not matching sips uri\n", (int) proto);
			goto error;
		}
		goto do_srv;
	}


	LM_DBG("no port, no proto -> do NAPTR lookup!\n");
	/* no proto, no port -> do NAPTR lookup */

	if (name->len >= MAX_DNS_NAME)
	{
		LM_ERR("domain name too long\n");
		goto error;
	}

	/* do NAPTR lookup */

	req->type = DNS_NAPTR_REQUEST;
	memcpy(req->name, name->s, name->len);
	req->name[name->len] = '\0';
	req->name_len = name->len;
	goto ok;

do_srv:

	if ((name->len + SRV_MAX_PREFIX_LEN + 1) > MAX_DNS_NAME)
	{
		LM_WARN("domain name too long (%d),"
				" unable to perform SRV lookup\n", name->len);
		/* set defaults */
		req->port = (is_sips) ? SIPS_PORT : SIP_PORT;
		goto do_a;
	}

	req->type = DNS_SRV_REQUEST;

	switch (proto)
	{
	case PROTO_UDP:
		memcpy(req->name, SRV_UDP_PREFIX, SRV_UDP_PREFIX_LEN);
		memcpy(req->name + SRV_UDP_PREFIX_LEN, name->s, name->len);
		req->name_len = SRV_UDP_PREFIX_LEN + name->len;
		req->name[req->name_len] = '\0';
		break;
	case PROTO_TCP:
		if (tcp_disable)
		{
			LM_ERR("tcp-disabled");
			goto error;
		}
		memcpy(req->name, SRV_TCP_PREFIX, SRV_TCP_PREFIX_LEN);
		memcpy(req->name + SRV_TCP_PREFIX_LEN, name->s, name->len);
		req->name_len = SRV_TCP_PREFIX_LEN + name->len;
		req->name[req->name_len] = '\0';
		break;
	case PROTO_TLS:
		if (tls_disable) goto error;
		memcpy(req->name, SRV_TLS_PREFIX, SRV_TLS_PREFIX_LEN);
		memcpy(req->name + SRV_TLS_PREFIX_LEN, name->s, name->len);
		req->name_len = SRV_TLS_PREFIX_LEN + name->len;
		req->name[req->name_len] = '\0';
		break;
	case PROTO_SCTP:
		if (sctp_disable) goto error;
		memcpy(req->name, SRV_SCTP_PREFIX, SRV_SCTP_PREFIX_LEN);
		memcpy(req->name + SRV_SCTP_PREFIX_LEN, name->s, name->len);
		req->name_len = SRV_SCTP_PREFIX_LEN + name->len;
		req->name[req->name_len] = '\0';
		break;
	default:
		LM_ERR("unsuitable protocol %d\n", (int) proto);
		goto error;
	}

	goto ok;

do_a:

	if (name->len >= MAX_DNS_NAME)
	{
		LM_ERR("domain name too long\n");
		goto error;
	}

	req->type = DNS_A_REQUEST;
	memcpy(req->name, name->s, name->len);
	req->name[name->len] = 0;

ok:
	next_he(req, func, arg);
	return;

error:
	func(arg, 0, PROTO_NONE, NULL, NULL);
}



void next_he(dns_request_t * requests, dns_sip_resolve_answer func, void * arg)
{
	sip_pack_t * pack = (sip_pack_t *) shm_malloc(sizeof (*pack));

	if (pack == NULL)
	{
		LM_ERR("Out of memory\n");
		goto error;
	}

	if (requests == NULL)
	{
		LM_WARN("Empty request list, no more options for fail-over\n");
		goto error;
	}

	pack->func = func;
	pack->arg = arg;
	pack->requests = requests;

	lock_get(&ares_lock);

	LM_DBG("Querying for %s with type %d\n", requests->name,
		requests->type);
	ares_query(channel, requests->name, ns_c_in,
			requests->type, sip_ares_to_dns, pack);
	lock_release(&ares_lock);

	return;

error:
	func(arg, 0, PROTO_NONE, NULL, NULL);

}

void print_requests(dns_request_t * l, int status)
{
	LM_DBG("return code is %d\n", status);
	while (l)
	{
		LM_DBG("Request type: %d for: %s\n", l->type, l->name);
		l = l->next;
	}
}

