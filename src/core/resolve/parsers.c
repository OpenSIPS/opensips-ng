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


#include "resolve.h"
#include "parsers.h"
#include <resolv.h>
#include "../net/proto.h"

/* mallocs for local stuff */
#define local_malloc shm_malloc
#define local_free   shm_free


/*! \brief parses the srv record into a srv_rdata structure
 *   \param msg   - pointer to the dns message
 *   \param end   - pointer to the end of the message
 *   \param rdata - pointer  to the rdata part of the srv answer
 *   \return 0 on error, or a dyn. alloc'ed srv_rdata structure
 *
 * SRV rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |     priority   |
 * |----------------|
 * |     weight     |
 * |----------------|
 * |   port number  |
 * |----------------|
 * |                |
 * ~      name      ~
 * |                |
 * +----------------+
 */
struct srv_rdata* dns_srv_parser(unsigned char* msg, unsigned char* end,
								 unsigned char* rdata)
{
	struct srv_rdata* srv;

	srv = 0;
	if ((rdata + 6) >= end) goto error;
	srv = (struct srv_rdata*) local_malloc(sizeof (struct srv_rdata));
	if (srv == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	memcpy((void*) & srv->priority, rdata, 2);
	memcpy((void*) & srv->weight, rdata + 2, 2);
	memcpy((void*) & srv->port, rdata + 4, 2);
	rdata += 6;
	srv->priority = ntohs(srv->priority);
	srv->weight = ntohs(srv->weight);
	srv->port = ntohs(srv->port);
	if ((srv->name_len = dn_expand(msg, end, rdata, srv->name, MAX_DNS_NAME - 1)) == -1)
		goto error;
	/* add terminating 0 ? (warning: len=compressed name len) */
	return srv;
error:
	if (srv) local_free(srv);
	return 0;
}

/*! \brief parses the naptr record into a naptr_rdata structure
 *   \param msg   - pointer to the dns message
 *   \param end   - pointer to the end of the message
 *   \param rdata - pointer  to the rdata part of the naptr answer
 *   \return  0 on error, or a dyn. alloc'ed naptr_rdata structure
 *
 * NAPTR rdata format:
 *            111111
 *  0123456789012345
 * +----------------+
 * |      order     |
 * |----------------|
 * |   preference   |
 * |----------------|
 * ~     flags      ~
 * |   (string)     |
 * |----------------|
 * ~    services    ~
 * |   (string)     |
 * |----------------|
 * ~    regexp      ~
 * |   (string)     |
 * |----------------|
 * ~  replacement   ~
   |    (name)      |
 * +----------------+
 */
struct naptr_rdata* dns_naptr_parser(unsigned char* msg, unsigned char* end,
									 unsigned char* rdata)
{
	struct naptr_rdata* naptr;

	naptr = 0;
	if ((rdata + 7) >= end)
		goto error;
	naptr = (struct naptr_rdata*) local_malloc(sizeof (struct naptr_rdata));
	if (naptr == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	memcpy((void*) & naptr->order, rdata, 2);
	naptr->order = ntohs(naptr->order);
	memcpy((void*) & naptr->pref, rdata + 2, 2);
	naptr->pref = ntohs(naptr->pref);
	naptr->flags_len = (int) rdata[4];
	if ((rdata + 7 + naptr->flags_len) >= end)
		goto error;
	memcpy((void*) & naptr->flags, rdata + 5, naptr->flags_len);
	naptr->services_len = (int) rdata[5 + naptr->flags_len];
	if ((rdata + 7 + naptr->flags_len + naptr->services_len) >= end) goto error;
	memcpy((void*) & naptr->services, rdata + 6 + naptr->flags_len, naptr->services_len);
	naptr->regexp_len = (int) rdata[6 + naptr->flags_len + naptr->services_len];
	if ((rdata + 7 + naptr->flags_len + naptr->services_len + naptr->regexp_len) >= end)
		goto error;
	memcpy((void*) & naptr->regexp, rdata + 7 + naptr->flags_len +
		naptr->services_len, naptr->regexp_len);
	rdata = rdata + 7 + naptr->flags_len + naptr->services_len + naptr->regexp_len;
	naptr->repl_len = dn_expand(msg, end, rdata, naptr->repl, MAX_DNS_NAME - 1);
	if (naptr->repl_len == (unsigned int) - 1)
		goto error;
	/* add terminating 0 ? (warning: len=compressed name len) */
	return naptr;
error:
	if (naptr)
		local_free(naptr);
	return 0;
}

/*! \brief Parses a CNAME record into a cname_rdata structure */
struct cname_rdata* dns_cname_parser(unsigned char* msg, unsigned char* end,
									 unsigned char* rdata)
{
	struct cname_rdata* cname;
	int len;

	cname = 0;
	cname = (struct cname_rdata*) local_malloc(sizeof (struct cname_rdata));
	if (cname == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	if ((len = dn_expand(msg, end, rdata, cname->name, MAX_DNS_NAME - 1)) == -1)
		goto error;
	return cname;
error:
	if (cname) local_free(cname);
	return 0;
}

/*! \brief Parses an A record rdata into an a_rdata structure
 * \return 0 on error or a dyn. alloc'ed a_rdata struct
 */
struct a_rdata* dns_a_parser(unsigned char* rdata, unsigned char* end)
{
	struct a_rdata* a;

	if (rdata + 4 >= end) goto error;
	a = (struct a_rdata*) local_malloc(sizeof (struct a_rdata));
	if (a == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	memcpy(a->ip, rdata, 4);

	return a;

error:
	return 0;
}

/*! \brief Parses an AAAA (ipv6) record rdata into an aaaa_rdata structure
 * \return 0 on error or a dyn. alloc'ed aaaa_rdata struct
 */
struct aaaa_rdata* dns_aaaa_parser(unsigned char* rdata, unsigned char* end)
{
	struct aaaa_rdata* aaaa;

	if (rdata + 16 >= end) goto error;
	aaaa = (struct aaaa_rdata*) local_malloc(sizeof (struct aaaa_rdata));
	if (aaaa == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}
	memcpy(aaaa->ip6, rdata, 16);
	return aaaa;
error:
	return 0;
}

/*! \brief Parses a TXT record into a txt_rdata structure
 * \note RFC1035:
 * - <character-string> is a single length octet followed by that number of characters.
 * - TXT-DATA        One or more <character-string>s.
 *
 * We only take the first string here.
 */
struct txt_rdata* dns_txt_parser(unsigned char* msg, unsigned char* end,
								 unsigned char* rdata)
{
	struct txt_rdata* txt;
	unsigned int len;

	txt = 0;
	txt = (struct txt_rdata*) local_malloc(sizeof (struct txt_rdata));
	if (txt == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	len = *rdata;
	if (rdata + 1 + len >= end)
		goto error; /*  something fishy in the record */
	if (len >= sizeof (txt->txt))
		goto error; /* not enough space? */
	memcpy(txt->txt, rdata + 1, len);
	txt->txt[len] = 0; /* 0-terminate string */
	return txt;

error:
	if (txt)
		local_free(txt);
	return 0;
}

/*! \brief parses a EBL record into a ebl_rdata structure
 *
 * EBL Record
 *
 *    0  1  2  3  4  5  6  7
 *    +--+--+--+--+--+--+--+--+
 *    |       POSITION        |
 *    +--+--+--+--+--+--+--+--+
 *    /       SEPARATOR       /
 *    +--+--+--+--+--+--+--+--+
 *    /         APEX          /
 *    +--+--+--+--+--+--+--+--+
 */
struct ebl_rdata* dns_ebl_parser(unsigned char* msg, unsigned char* end,
								 unsigned char* rdata)
{
	struct ebl_rdata* ebl;
	int len;

	ebl = 0;
	ebl = (struct ebl_rdata*) local_malloc(sizeof (struct ebl_rdata));
	if (ebl == 0)
	{
		LM_ERR("out of pkg memory\n");
		goto error;
	}

	len = *rdata;
	if (rdata + 1 + len >= end)
		goto error; /*  something fishy in the record */

	ebl->position = *rdata;
	if (ebl->position > 15)
		goto error; /* doesn't make sense: E.164 numbers can't be longer */

	rdata++;

	ebl->separator_len = (int) * rdata;
	rdata++;
	if ((rdata + 1 + ebl->separator_len) >= end)
		goto error;
	memcpy((void*) & ebl->separator, rdata, ebl->separator_len);
	rdata += ebl->separator_len;

	ebl->apex_len = dn_expand(msg, end, rdata, ebl->apex, MAX_DNS_NAME - 1);
	if (ebl->apex_len == (unsigned int) - 1)
		goto error;
	ebl->apex[ebl->apex_len] = 0; /* 0-terminate string */
	return ebl;

error:
	if (ebl)
		local_free(ebl);
	return 0;
}


/*! \brief Skips over a domain name in a dns message
 *  (it can be  a sequence of labels ending in \\0, a pointer or
 *   a sequence of labels ending in a pointer -- see rfc1035
 *   returns pointer after the domain name or null on error
 */
unsigned char* dns_skipname(unsigned char* p, unsigned char* end)
{
	while (p < end)
	{
		/* check if \0 (root label length) */
		if (*p == 0)
		{
			p += 1;
			break;
		}
		/* check if we found a pointer */
		if (((*p)&0xc0) == 0xc0)
		{
			/* if pointer skip over it (2 bytes) & we found the end */
			p += 2;
			break;
		}
		/* normal label */
		p += *p + 1;
	}
	return (p >= end) ? 0 : p;
}


/*! \brief frees completely a struct rdata list */
void free_rdata_list(struct rdata* head)
{
	struct rdata* l;
	struct rdata* next_l;

	for (l = head; l; l = next_l)
	{
		next_l = l->next;
		/* free the parsed rdata*/
		if (l->rdata)
			local_free(l->rdata);
		local_free(l);
	}
}


int get_naptr_proto(struct naptr_rdata *n)
{
#ifdef USE_TLS
	if (n->services[3] == 's' || n->services[3] == 'S')
		return PROTO_TLS;
#endif
	switch (n->services[n->services_len - 1])
	{
	case 'U':
	case 'u':
		return PROTO_UDP;
		break;
#ifdef USE_TCP
	case 'T':
	case 't':
		return PROTO_TCP;
		break;
#endif
#ifdef USE_SCTP
	case 'S':
	case 's':
		return PROTO_SCTP;
		break;
#endif

	}
	LM_CRIT("failed to detect proto\n");
	return PROTO_NONE;
}

struct rdata* parse_record(unsigned char* reply, int size)
{
	int qno, answers_no;
	int r;
	int ans_len;
	HEADER  hdr ;
	unsigned char* p;

	unsigned char* end;
	unsigned short rtype, class, rdlength;
	unsigned int ttl;
	struct rdata* head;
	struct rdata** crt;
	struct rdata** last;
	struct rdata* rd;
	struct srv_rdata* srv_rd;
	struct srv_rdata* crt_srv;

	head = rd = 0;
	last = crt = &head;

	memcpy(&hdr,reply, sizeof(hdr));

	p = reply + DNS_HDR_SIZE;
	end = reply + size;
	if (p >= end) goto error_boundary;
	qno = ntohs((unsigned short) hdr.qdcount);

	for (r = 0; r < qno; r++)
	{
		/* skip the name of the question */
		if ((p = dns_skipname(p, end)) == 0)
		{
			LM_ERR("skipname==0\n");
			goto error;
		}
		p += 2 + 2; /* skip QCODE & QCLASS */
#if 0
		for (; (p < end && (*p)); p++);
		p += 1 + 2 + 2; /* skip the ending  '\0, QCODE and QCLASS */
#endif
		if (p >= end)
		{
			LM_ERR("p>=end\n");
			goto error;
		}
	};
	answers_no = ntohs((unsigned short) hdr.ancount);
	ans_len = ANS_SIZE;

	for (r = 0; (r < answers_no) && (p < end); r++)
	{
		/*  ignore it the default domain name */
		if ((p = dns_skipname(p, end)) == 0)
		{
			LM_ERR("skip_name=0 (#2)\n");
			goto error;
		}
		
		/* check if enough space is left for type, class, ttl & size */
		if ((p + 2 + 2 + 4 + 2) >= end) goto error_boundary;
		/* get type */
		memcpy((void*) & rtype, (void*) p, 2);
		rtype = ntohs(rtype);
		p += 2;
		/* get  class */
		memcpy((void*) & class, (void*) p, 2);
		class = ntohs(class);
		p += 2;
		/* get ttl*/
		memcpy((void*) & ttl, (void*) p, 4);
		ttl = ntohl(ttl);
		p += 4;
		/* get size */
		memcpy((void*) & rdlength, (void*) p, 2);
		rdlength = ntohs(rdlength);
		p += 2;
		/* check for type */
		
		/* expand the "type" record  (rdata)*/

		rd = (struct rdata*) local_malloc(sizeof (struct rdata));
		if (rd == 0)
		{
			LM_ERR("out of pkg memory\n");
			goto error;
		}
		rd->type = rtype;
		rd->class = class;
		rd->ttl = ttl;
		rd->next = 0;
		switch (rtype)
		{
		case T_SRV:
			srv_rd = dns_srv_parser(reply, end, p);
			rd->rdata = (void*) srv_rd;
			if (srv_rd == 0) goto error_parse;

			/* insert sorted into the list */
			for (crt = &head; *crt; crt = &((*crt)->next))
			{
				crt_srv = (struct srv_rdata*) (*crt)->rdata;
				if ((srv_rd->priority < crt_srv->priority) ||
					((srv_rd->priority == crt_srv->priority) &&
					((srv_rd->weight == 0) || (crt_srv->weight != 0))))
				{
					/* insert here */
					goto skip;
				}
			}
			last = &(rd->next); /*end of for => this will be the last elem*/
skip:
			/* insert here */
			rd->next = *crt;
			*crt = rd;

			break;
		case T_A:
			rd->rdata = (void*) dns_a_parser(p, end);
			if (rd->rdata == 0) goto error_parse;
			*last = rd; /* last points to the last "next" or the list head*/
			last = &(rd->next);
			break;
		case T_AAAA:
			rd->rdata = (void*) dns_aaaa_parser(p, end);
			if (rd->rdata == 0) goto error_parse;
			*last = rd;
			last = &(rd->next);
			break;
		case T_CNAME:
			rd->rdata = (void*) dns_cname_parser(reply, end, p);
			if (rd->rdata == 0) goto error_parse;
			*last = rd;
			last = &(rd->next);
			break;
		case T_NAPTR:
			rd->rdata = (void*) dns_naptr_parser(reply, end, p);
			if (rd->rdata == 0) goto error_parse;
			*last = rd;
			last = &(rd->next);
			break;
		case T_TXT:
			rd->rdata = (void*) dns_txt_parser(reply, end, p);
			if (rd->rdata == 0) goto error_parse;
			*last = rd;
			last = &(rd->next);
			break;
		case T_EBL:
			rd->rdata = (void*) dns_ebl_parser(reply, end, p);
			if (rd->rdata == 0) goto error_parse;
			*last = rd;
			last = &(rd->next);
			break;
		default:
			LM_ERR("unknown type %d\n", rtype);
			rd->rdata = 0;
			*last = rd;
			last = &(rd->next);
		}

		p += rdlength;

	}
	return head;
error_boundary:
	LM_ERR("end of query buff reached\n");
	if (head)
		free_rdata_list(head);
	return 0;
error_parse:
	LM_ERR("rdata parse error \n");
	if (rd) local_free(rd); /* rd->rdata=0 & rd is not linked yet into
								   the list */
error:
	LM_ERR("get_record \n");
	if (head) free_rdata_list(head);
	return 0;
}


#define naptr_prio(_naptr) \
	((unsigned int)((((_naptr)->order) << 16) + ((_naptr)->pref)))



void filter_and_sort_naptr(struct rdata** head_p, struct rdata** filtered_p, int is_sips)
{
	struct naptr_rdata *naptr;
	struct rdata *head;
	struct rdata *last;
	struct rdata *out;
	struct rdata *l, *ln, *it, *itp;
	unsigned int prio;
	char p;

	head = 0;
	last = 0;
	out = 0;

	for (l = *head_p; l; l = ln)
	{
		ln = l->next;

		if (l->type != T_NAPTR)
			goto skip0; /*should never happen*/

		naptr = (struct naptr_rdata*) l->rdata;
		if (naptr == 0)
		{
			LM_CRIT("null rdata\n");
			goto skip0;
		}

		LM_INFO("%s %d\n",naptr->services, naptr->services_len);

		/* first filter out by flag and service */
		if (naptr->flags_len != 1 || (naptr->flags[0] != 's' && naptr->flags[0] != 'S'))
			goto skip;
		if (naptr->repl_len == 0 || naptr->regexp_len != 0)
			goto skip;
		if ((is_sips || naptr->services_len != 7 ||
			strncasecmp(naptr->services, "sip+d2", 6)) &&
			( !is_sips ||
				naptr->services_len != 8 || strncasecmp(naptr->services, "sips+d2", 7)))
			goto skip;
		p = naptr->services[naptr->services_len - 1];
		/* by default we do not support SCTP */
		if (p != 'U' && p != 'u'
#ifdef USE_TCP
				&& (tcp_disable || (p != 'T' && p != 't'))
#endif
#ifdef USE_SCTP
				&& (sctp_disable || (p != 'S' && p != 's'))
#endif
				)
			goto skip;
		/* is it valid? (SIPS+D2U is not!) */
		if (naptr->services_len == 8 && (p == 'U' || p == 'u'))
			goto skip;

		LM_DBG("found valid %.*s -> %s\n",
			(int) naptr->services_len, naptr->services, naptr->repl);

		/* this is a supported service -> add it according to order to the
		 * new head list */
		prio = naptr_prio(get_naptr(l));
		if (head == 0)
		{
			head = last = l;
			l->next = 0;
		} else if (naptr_prio(get_naptr(head)) >= prio)
		{
			l->next = head;
			head = l;
		} else if (prio >= naptr_prio(get_naptr(last)))
		{
			l->next = 0;
			last->next = l;
			last = l;
		} else
		{
			for (itp = head, it = head->next; it && it->next; itp = it, it = it->next)
			{
				if (prio <= naptr_prio(get_naptr(it)))
					break;
			}
			l->next = itp->next;
			itp->next = l;
		}

		continue;
skip:
		LM_DBG("skipping %.*s -> %s\n",
			(int) naptr->services_len, naptr->services, naptr->repl);
skip0:
		l->next = out;
		out = l;
	}

	*head_p = head;
	*filtered_p = out;
}


void sort_srvs(struct rdata **head)
{
#define rd2srv(_rd) ((struct srv_rdata*)_rd->rdata)
	struct rdata *rd = *head;
	struct rdata *tail = NULL;
	struct rdata *rd_next;
	struct rdata *crt;
	struct rdata *crt2;
	unsigned int weight_sum;
	unsigned int rand_no;


	*head = NULL;

	while (rd)
	{
		rd_next = rd->next;
		if (rd->type != T_SRV)
		{
			rd->next = NULL;
			free_rdata_list(rd);
		} else
		{
			/* only on element with same priority ? */
			if (rd_next == NULL ||
				rd2srv(rd)->priority != rd2srv(rd_next)->priority)
			{
				if (tail)
				{
					tail->next = rd;
					tail = rd;
				} else
				{
					*head = tail = rd;
				}
				rd->next = NULL;
			} else
			{
				/* multiple nodes with same priority */
				/* -> calculate running sums (and detect the end) */
				weight_sum = rd2srv(rd)->running_sum = rd2srv(rd)->weight;
				crt = rd;
				while (crt && crt->next &&
					(rd2srv(rd)->priority == rd2srv(crt->next)->priority))
				{
					crt = crt->next;
					weight_sum += rd2srv(crt)->weight;
					rd2srv(crt)->running_sum = weight_sum;
				}
				/* crt will point to last RR with same priority */
				rd_next = crt->next;
				crt->next = NULL;

				/* order the elements between rd and crt */
				while (rd->next)
				{
					rand_no = (unsigned int)
							(weight_sum * ((float) rand() / RAND_MAX));
					for (crt = rd, crt2 = NULL; crt; crt2 = crt, crt = crt->next)
					{
						if (rd2srv(crt)->running_sum >= rand_no) break;
					}
					if (crt == NULL)
					{
						LM_CRIT("bug in sorting SRVs - rand>sum\n");
						crt = rd;
						crt2 = NULL;
					}
					/* remove the element from the list ... */
					if (crt2 == NULL)
					{
						rd = rd->next;
					} else
					{
						crt2->next = crt->next;
					}
					/* .... and update the running sums */
					for (crt2 = crt->next; crt2; crt2 = crt2->next)
						rd2srv(crt2)->running_sum -= rd2srv(crt)->weight;
					weight_sum -= rd2srv(crt)->weight;
					/* link the crt in the new list */
					crt->next = 0;
					if (tail)
					{
						tail->next = crt;
						tail = crt;
					} else
					{
						*head = tail = crt;
					}
				}
				/* just insert the last remaining element */
				tail->next = rd;
				tail = rd;
			}
		}

		rd = rd_next;
	}
}

