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


#ifndef _CORE_BUILDER_MSG_BUILDER_H
#define _CORE_BUILDER_MSG_BUILDER_H

#include "../parser/msg_parser.h"

struct hdr_field* add_hdr(struct sip_msg *msg,str *name,str *body,
		hdr_types_t type,struct hdr_field* after,int flags);

int rm_hdr(struct sip_msg *msg,struct hdr_field *removed);

int replace_hdr(struct sip_msg *msg,struct hdr_field *hdr,str *body);


int alter_hdr(struct sip_msg *msg, struct hdr_field *hdr, int offset, int len,
		str *body);

char *construct_msg(struct sip_msg *msg, int *len);

#endif

