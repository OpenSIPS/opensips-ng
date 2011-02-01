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
 *  2010-11-xx  created (vlad)
 */

#include "msg_builder.h"
#include "../parser/parse_hname2.h"

struct hdr_field* add_hdr(struct sip_msg *msg,str *name,str *body,
		hdr_types_t type,struct hdr_field* after,int flags)
{
	struct hdr_field *new,*itr;
	int size_mem;
	int name_pos = 0;

#define link_sibling_hdr(_hook, _hdr) \
	do{ \
		if (msg->_hook==0) msg->_hook=_hdr;\
			else {\
				for(itr=msg->_hook;itr->next_sibling;itr=itr->next_sibling);\
				itr->next_sibling = _hdr;\
				_hdr->prev_sibling = itr; \
			}\
	}while(0)

	if (msg == 0)
	{
		LM_ERR("null message provided \n");
		return 0;
	}

	size_mem = sizeof(struct hdr_field);

	if (flags & HDR_DUP_NAME)
	{
		name_pos = size_mem;
		size_mem += name->len;
	}

	new = shm_malloc(size_mem);

	if (new == 0)
	{
		LM_ERR("no more memory !\n");
		return 0;
	}

	memset(new,0,size_mem);
	new->name.len = name->len;
	new->body.len = body->len;
	new->body_buff_size = body->len;
	new->flags = flags;

	if (flags & HDR_DUP_NAME)
	{
		if (name == 0 || name->s == 0)
		{
			LM_ERR("HDR_DUP_NAME sent, but null string provided \n");
			shm_free(new);
			return 0;
		}
		else
		{
			new->name.s = ((char *)new) + name_pos;
			strncpy(new->name.s,name->s,name->len);
		}
	}
	else
		new->name.s = name->s;

	if (flags & HDR_DUP_BODY)
	{
		new->body.s = shm_malloc(body->len);
		if (new->body.s == 0)
		{
			LM_ERR("no more memory !\n");
			shm_free(new);
			return 0;
		}
		/* forced to set free flag */
		new->flags |= HDR_FREE_BODY;
		strncpy(new->body.s,body->s,body->len);
	}
	else
		new->body.s = body->s;

	if (type < 0)
		parse_name_only(name->s,name->s+name->len,new);
	else
		new->type = type;

	new->flags |= HDR_NOT_ORIG_MSG;

	switch (new->type)
	{
		case HDR_CALLID_T:
			if (msg->callid == 0) msg->callid = new;
			break;
		case HDR_TO_T:
			if (msg->to == 0) msg->to = new;
			break;
		case HDR_CSEQ_T:
			if (msg->cseq == 0) msg->cseq = new;
			break;
		case HDR_FROM_T:
			if (msg->from == 0) msg->from = new;
			break;
		case HDR_CONTACT_T:
			link_sibling_hdr(contact, new);
			break;
		case HDR_MAXFORWARDS_T:
			if (msg->maxforwards == 0) msg->maxforwards = new;
			break;
		case HDR_ROUTE_T:
			link_sibling_hdr(route, new);
			break;
		case HDR_RECORDROUTE_T:
			link_sibling_hdr(record_route, new);
			break;
		case HDR_PATH_T:
			link_sibling_hdr(path, new);
			break;
		case HDR_CONTENTTYPE_T:
			if (msg->content_type == 0) msg->content_type = new;
			break;
		case HDR_CONTENTLENGTH_T:
			if (msg->content_length == 0) msg->content_length = new;
			break;
		case HDR_AUTHORIZATION_T:
			link_sibling_hdr(authorization, new);
			break;
		case HDR_EXPIRES_T:
			if (msg->expires == 0) msg->expires = new;
			break;
		case HDR_PROXYAUTH_T:
			link_sibling_hdr(proxy_auth, new);
			break;
		case HDR_PROXYREQUIRE_T:
			link_sibling_hdr(proxy_require, new);
			break;
		case HDR_SUPPORTED_T:
			link_sibling_hdr(supported, new);
			break;
		case HDR_UNSUPPORTED_T:
			link_sibling_hdr(unsupported, new);
			break;
		case HDR_ALLOW_T:
			link_sibling_hdr(allow, new);
			break;
		case HDR_EVENT_T:
			link_sibling_hdr(event, new);
			break;
		case HDR_ACCEPT_T:
			link_sibling_hdr(accept, new);
			break;
		case HDR_ACCEPTLANGUAGE_T:
			link_sibling_hdr(accept_language, new);
			break;
		case HDR_ORGANIZATION_T:
			if (msg->organization == 0) msg->organization = new;
			break;
		case HDR_PRIORITY_T:
			if (msg->priority == 0) msg->priority = new;
			break;
		case HDR_SUBJECT_T:
			if (msg->subject == 0) msg->subject = new;
			break;
		case HDR_USERAGENT_T:
			if (msg->user_agent == 0) msg->user_agent = new;
			break;
		case HDR_CONTENTDISPOSITION_T:
			if (msg->content_disposition == 0) msg->content_disposition = new;
			break;
		case HDR_ACCEPTDISPOSITION_T:
			link_sibling_hdr(accept_disposition, new);
			break;
		case HDR_DIVERSION_T:
			link_sibling_hdr(diversion, new);
			break;
		case HDR_RPID_T:
			if (msg->rpid == 0) msg->rpid = new;
			break;
		case HDR_REFER_TO_T:
			if (msg->refer_to == 0) msg->refer_to = new;
			break;
		case HDR_SESSION_EXPIRES_T:
			if (msg->session_expires == 0) msg->session_expires = new;
			break;
		case HDR_MIN_SE_T:
			if (msg->min_se == 0) msg->min_se = new;
			break;
		case HDR_PPI_T:
			if (msg->ppi == 0) msg->ppi = new;
			break;
		case HDR_PAI_T:
			if (msg->pai == 0) msg->pai = new;
			break;
		case HDR_PRIVACY_T:
			if (msg->privacy == 0) msg->privacy = new;
			break;
		case HDR_RETRY_AFTER_T:
			break;
		case HDR_VIA_T:
			link_sibling_hdr(h_via, new);
			if (msg->h_via == 0)
				/* TODO - should also parse and set msg->via1 ??? */
				msg->h_via = new;
			break;
		case HDR_OTHER_T: /*do nothing*/
			break;
		case HDR_ERROR_T:
		default:
			LM_ERR("bad header type %d\n", type);
			shm_free(new);
			return 0;
		}

	if (after == NULL)
	{
		/* add to head of list */
		if (msg->headers != 0)
		{
			new->next = msg->headers;
			msg->headers->prev = new;
			msg->headers = new;
		}
		else
		{
			msg->headers = new;
		}
	}
	else
	{
		new->next = after->next;
		new->prev = after;
		after->next = new;
		if (after->next)
			after->next->prev = new;
		else
		{
			/* after = msg->last_header */
			msg->last_header = new;
		}
	}

	/* 2 extra bytes for ": " between name and body,
	 * that will be added when building the final message */
	msg->new_len += new->name.len + new->body.len + 2;
	/* TODO - setting length to EOH to -1 ?*/
	new->len = -1;
	return new;
}


#define detach_sibling_hdr(_hook, _hdr) \
	do{ \
			if (msg->_hook==_hdr) { \
				msg->_hook=0; \
				if (_hdr->next_sibling) { \
					_hdr->next_sibling->prev_sibling = 0;\
					msg->_hook = _hdr->next_sibling; \
				} \
			} \
			else {\
				_hdr->prev_sibling->next_sibling = _hdr->next_sibling; \
				if (_hdr->next_sibling) \
					_hdr->next_sibling->prev_sibling = _hdr->prev_sibling; \
			}\
	}while(0)

int rm_hdr(struct sip_msg *msg,struct hdr_field *removed)
{
	if (msg == 0)
	{
		LM_ERR("null message provided \n");
		return -1;
	}

	if (msg->headers == 0)
	{
		LM_ERR("null headers list \n");
		return -1;
	}

	if (removed == 0)
	{
		LM_ERR("null hdr_field ! cannot remove\n");
		return -1;
	}

	if (removed->flags & HDR_FREE_NAME)
		shm_free(removed->name.s);

	if (removed->flags & HDR_FREE_BODY)
		shm_free(removed->body.s);

	if (removed->flags & HDR_NOT_ORIG_MSG)
	/* 2 extra bytes for ": " between name and body,
	 * that will be added when building the final message */
		msg->new_len -= (removed->name.len + removed->body.len + 2);
	else
		msg->new_len -= removed->len;

	clean_hdr_field(removed);

	if (removed == msg->headers)
	{
		/* remove head of list */
		msg->headers = removed->next;
		if (removed->next)
			removed->next->prev = 0;
	}
	else
	{
		removed->prev->next = removed->next;
		if (removed->next)
			removed->next->prev = removed->prev;
		else
		{
			/* removed = msg->last_header */
			msg->last_header = removed->prev;
		}
	}

	switch (removed->type)
	{
		case HDR_CALLID_T:
			if (msg->callid == removed) msg->callid = 0;
			break;
		case HDR_TO_T:
			if (msg->to == removed) msg->to = 0;
			break;
		case HDR_CSEQ_T:
			if (msg->cseq == removed) msg->cseq = 0;
			break;
		case HDR_FROM_T:
			if (msg->from == removed) msg->from = 0;
			break;
		case HDR_CONTACT_T:
			detach_sibling_hdr(contact, removed);
			break;
		case HDR_MAXFORWARDS_T:
			if (msg->maxforwards == removed) msg->maxforwards = 0;
			break;
		case HDR_ROUTE_T:
			detach_sibling_hdr(route, removed);
			break;
		case HDR_RECORDROUTE_T:
			detach_sibling_hdr(record_route, removed);
			break;
		case HDR_PATH_T:
			detach_sibling_hdr(path, removed);
			break;
		case HDR_CONTENTTYPE_T:
			if (msg->content_type == removed) msg->content_type = 0;
			break;
		case HDR_CONTENTLENGTH_T:
			if (msg->content_length == removed) msg->content_length = 0;
			break;
		case HDR_AUTHORIZATION_T:
			detach_sibling_hdr(authorization, removed);
			break;
		case HDR_EXPIRES_T:
			if (msg->expires == removed) msg->expires = 0;
			break;
		case HDR_PROXYAUTH_T:
			detach_sibling_hdr(proxy_auth, removed);
			break;
		case HDR_PROXYREQUIRE_T:
			detach_sibling_hdr(proxy_require, removed);
			break;
		case HDR_SUPPORTED_T:
			detach_sibling_hdr(supported, removed);
			break;
		case HDR_UNSUPPORTED_T:
			detach_sibling_hdr(unsupported, removed);
			break;
		case HDR_ALLOW_T:
			detach_sibling_hdr(allow, removed);
			break;
		case HDR_EVENT_T:
			detach_sibling_hdr(event, removed);
			break;
		case HDR_ACCEPT_T:
			detach_sibling_hdr(accept, removed);
			break;
		case HDR_ACCEPTLANGUAGE_T:
			detach_sibling_hdr(accept_language, removed);
			break;
		case HDR_ORGANIZATION_T:
			if (msg->organization == removed) msg->organization = 0;
			break;
		case HDR_PRIORITY_T:
			if (msg->priority == removed) msg->priority = 0;
			break;
		case HDR_SUBJECT_T:
			if (msg->subject == removed) msg->subject = 0;
			break;
		case HDR_USERAGENT_T:
			if (msg->user_agent == removed) msg->user_agent = 0;
			break;
		case HDR_CONTENTDISPOSITION_T:
			if (msg->content_disposition==removed) msg->content_disposition=0;
			break;
		case HDR_ACCEPTDISPOSITION_T:
			detach_sibling_hdr(accept_disposition, removed);
			break;
		case HDR_DIVERSION_T:
			detach_sibling_hdr(diversion, removed);
			break;
		case HDR_RPID_T:
			if (msg->rpid == removed) msg->rpid = 0;
			break;
		case HDR_REFER_TO_T:
			if (msg->refer_to == removed) msg->refer_to = 0;
			break;
		case HDR_SESSION_EXPIRES_T:
			if (msg->session_expires == removed) msg->session_expires = 0;
			break;
		case HDR_MIN_SE_T:
			if (msg->min_se == removed) msg->min_se = 0;
			break;
		case HDR_PPI_T:
			if (msg->ppi == removed) msg->ppi = 0;
			break;
		case HDR_PAI_T:
			if (msg->pai == removed) msg->pai = 0;
			break;
		case HDR_PRIVACY_T:
			if (msg->privacy == removed) msg->privacy = 0;
			break;
		case HDR_RETRY_AFTER_T:
			break;
		case HDR_VIA_T:
			detach_sibling_hdr(h_via, removed);
			break;
		case HDR_OTHER_T: /*do nothing*/
			break;
		case HDR_ERROR_T:
		default:
			LM_ERR("Unexpected header type %d\n",removed->type);
	}

	shm_free(removed);
	return 0;
}

/* new body MUST end with \r\n ! */
int replace_hdr(struct sip_msg *msg,struct hdr_field *hdr,str *body)
{
	if (hdr == 0)
	{
		LM_ERR("null header field provide \n");
		return -1;
	}

	if (body == 0)
	{
		LM_ERR("null pointer provided for replace_hdr\n");
		return -1;
	}

	if (body->s == 0)
	{
		LM_ERR("new body can't be null\n");
		return -1;
	}

	if (hdr->flags & HDR_NOT_ORIG_MSG)
	{
		/* if not original message and shouldn't free buffer,
		 * than the buffer is static 
		 */
		if (!(hdr->flags & HDR_FREE_BODY))
			goto allocate;
		/* explicitly added headers */
		if (hdr->body_buff_size < body->len)
		{
			hdr->body.s = shm_realloc(hdr->body.s,body->len);
			if (hdr->body.s == 0)
			{
				LM_ERR("no more memory !\n");
				return -1;
			}
			hdr->body_buff_size = body->len;
		}

		msg->new_len += (body->len - hdr->body.len);
	}
	else
	{
		/* header from the initial message, or static buffer. 
		 * either way, allocate new chunk of mem */
allocate:
		hdr->body.s = shm_malloc(body->len);
		if (hdr->body.s == 0)
		{
			LM_ERR("no more memory !\n");
			return -1;
		}
		hdr->flags |= HDR_FREE_BODY | HDR_NOT_ORIG_MSG;
		hdr->body_buff_size = body->len;

		msg->new_len -= hdr->len;
		/* 2 extra bytes for ": " between name and body,
		 * that will be added when building the final message */
		msg->new_len += hdr->name.len + body->len + 2;
	}

	hdr->body.len = body->len;
	memcpy(hdr->body.s,body->s,body->len);
	clean_hdr_field(hdr);
	hdr->parsed = 0;

	/* TODO - setting length to EOH to -1 */
	hdr->len = -1;

	return 0;
}

/* alter hdr body, starting from offset,
 * removing len characters, and adding body->s
 *
 * if body is null, just remove len characters starting from offset
 */
int alter_hdr(struct sip_msg *msg, struct hdr_field *hdr, int offset, int len,
																str *body)
{
	int new_body_length = body?body->len:0;
	char **hdr_body = &(hdr->body.s);
	char *old_body;
	int *hdr_body_len = &(hdr->body.len);
	int new_buf_size;

	if (hdr == 0)
	{
		LM_ERR("Null header field provided \n");
		return -1;
	}

	if (offset + len > *hdr_body_len)
	{
		LM_ERR("body would overflow\n");
		return -1;
	}

	if (hdr->flags & HDR_NOT_ORIG_MSG)
	{
		/* if not original message and shouldn't free buffer,
		 * than the buffer is static 
		 */
		if (!(hdr->flags & HDR_FREE_BODY))
			goto allocate;

		/* explicitly added headers */
		new_buf_size = *hdr_body_len + new_body_length - len;
		if (body && new_buf_size > hdr->body_buff_size)
		{
			hdr->body_buff_size = new_buf_size;

			*hdr_body = shm_realloc(*hdr_body,hdr->body_buff_size);
			if (*hdr_body == 0)
			{
				LM_ERR("no more memory !\n");
				return -1;
			}
		}

		memmove(*hdr_body + offset + new_body_length,*hdr_body + offset+len,
				*hdr_body_len - offset - len);
		if (new_body_length)
			memcpy(*hdr_body + offset, body->s,new_body_length);
		*hdr_body_len += (new_body_length - len);
		msg->new_len += body->len - len;
	}
	else
	{
		/* header from the initial message, or static buffer
		 * either way, allocate new chunk of mem*/
allocate:
		old_body = *hdr_body;
		/* also make sure to allocate 2 extra bytes for \r\n
		 * which will otherwise be lost */
		hdr->body_buff_size = *hdr_body_len + new_body_length - len + 2;
		*hdr_body = shm_malloc(hdr->body_buff_size);
		if (*hdr_body == 0)
		{
			LM_ERR("no more memory \n");
			return -1;
		}

		memcpy(*hdr_body,old_body,offset);
		memcpy(*hdr_body + offset + new_body_length,old_body + offset+len,
				*hdr_body_len - offset - len);
		if (new_body_length)
			memcpy(*hdr_body + offset, body->s,new_body_length);

		*hdr_body_len += (new_body_length - len) + 2;
		(*hdr_body)[*hdr_body_len-1] = '\n';
		(*hdr_body)[*hdr_body_len-2] = '\r';

		hdr->flags |= HDR_FREE_BODY | HDR_NOT_ORIG_MSG;

		msg->new_len -= hdr->len;
		/* 2 extra bytes for ": " between name and body,
		 * that will be added when building the final message */
		msg->new_len += hdr->name.len + *hdr_body_len + 2;
	}

	clean_hdr_field(hdr);
	hdr->parsed = 0;
	/* TODO - setting length to EOH to -1 */
	hdr->len = -1;
	return 0;
}


char *construct_msg(struct sip_msg *msg,int *len)
{
	struct hdr_field *itr;
	char *buf;
	int to_copy = 0, pos=0,body_size = 0,size,rest;
	char *copy_start = 0;
	int new_len,uri_len=0;

	if (msg == 0)
	{
		LM_ERR("null pointer provided to construct_msg\n");
		return 0;
	}

	body_size = (msg->buf + msg->len) - msg->unparsed; // FIXME
	new_len = msg->new_len;
	if (msg->new_uri.s)
	{
		uri_len = msg->new_uri.len;
		new_len = new_len - msg->first_line.u.request.uri.len + uri_len;
	}

	*len = new_len;
	buf = shm_malloc(new_len);
	if (buf == 0)
	{
		LM_ERR("no more memory !\n");
		return 0;
	}

	if (msg->new_uri.s)
	{
		/* copy up to URI */
		size=msg->first_line.u.request.uri.s-msg->buf;
		memcpy(buf, msg->buf, size);
		pos+= size;
		/* copy our new URI */
		memcpy(buf+pos, msg->new_uri.s, uri_len);
		pos += uri_len;
		/* copy rest of request line */
		rest = msg->first_line.len - size - msg->first_line.u.request.uri.len;
		memcpy(buf+pos,msg->first_line.u.request.uri.s +
				msg->first_line.u.request.uri.len,rest);
		pos+= rest;
	}
	else
	{
		memcpy(buf,msg->buf,msg->first_line.len);
		pos+= msg->first_line.len;
	}

	for (itr=msg->headers;itr;itr=itr->next)
	{
		if (!(itr->flags & HDR_NOT_ORIG_MSG))
		{
			/* name and body are in a single buffer */
			if (to_copy == 0)
				copy_start = itr->name.s;
			to_copy += itr->len;

			if (itr->next == 0)
			{
				/* last header wasn't modified
				 * force copy now 
				 */
				memcpy(buf+pos,copy_start,to_copy);
				pos += to_copy;
				goto copy_body;
			}
			
			/* maybe more consecutive headers from original msg */
			continue;
		}

		/* header that has been newly added or altered 
		 * name and body are separate buffers */

		if (to_copy != 0)
		{
			/* first copy previous original headers */
			memcpy(buf+pos,copy_start,to_copy);
			pos += to_copy;
			to_copy = 0;
		}

		memcpy(buf+pos,itr->name.s,itr->name.len);
		pos += itr->name.len;
		buf[pos] = ':';
		buf[pos+1] = ' ';
		pos +=2;
		memcpy(buf+pos,itr->body.s,itr->body.len);
		pos += itr->body.len;

	}

copy_body:
	memcpy(buf+pos,msg->unparsed,body_size);
	return buf;
}


