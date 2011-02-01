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
 *  2010-03-xx  created (bogdan)
 */



#ifndef _CORE_CONTEXT_H
#define _CORE_CONTEXT_H

#include "parser/msg_parser.h"

struct osips_ctx;

typedef int (resume_ctx_f)(struct osips_ctx*, void *, int );

struct osips_ctx {
	struct sip_msg *msg;
	resume_ctx_f* resume_f;
	void * resume_param;
};

#ifdef __SUNPRO_C

	#define async( _ctx, _resume_f, _resume_param, _n, _async_f, ... ) \
	do { \
		(_ctx)->resume_f = _resume_f; \
		(_ctx)->resume_param = _resume_param; \
		if ( (_n=_async_f(_ctx, __VA_ARGS__))!=1 ) {\
			_resume_f(_ctx,_resume_param,_n); \
		}\
		/* asyc */ \
	}while(0)

#else

	#define async( _ctx, _resume_f, _resume_param, _n, _async_f, args... ) \
	do { \
		(_ctx)->resume_f = _resume_f; \
		(_ctx)->resume_param = _resume_param; \
		if ( (_n=_async_f(_ctx, ##args))!=1 ) {\
			_resume_f(_ctx,_resume_param,_n); \
		}\
		/* asyc */ \
	}while(0)

#endif

struct osips_ctx *context_create(struct sip_msg *msg);

void context_destroy(struct osips_ctx *ctx);

#endif

