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


#include "mem/mem.h"
#include "log.h"
#include "context.h"


struct osips_ctx *context_create(struct sip_msg *msg)
{
	struct osips_ctx *ctx;

	ctx = (struct osips_ctx*)shm_malloc( sizeof(struct osips_ctx) );
	if (ctx==NULL) {
		LM_ERR("no more shm memory\n");
		return NULL;
	}
	memset( ctx, 0, sizeof(struct osips_ctx));
	ctx->msg = msg;
	return ctx;
}


void context_destroy(struct osips_ctx *ctx)
{
	if (ctx->msg)
		free_sip_msg(ctx->msg);
	shm_free(ctx);
}


void context_resume( void *p_ctx, int ret_code)
{
	struct osips_ctx *ctx = (struct osips_ctx *)p_ctx;

	LM_DBG("resuming contect %p\n",ctx);

	if (ctx->resume_f==NULL) {
		LM_DBG("ctx has no resume function -> context ended\n");
		context_destroy( ctx );
		return;
	}
	ctx->resume_f( ctx, ctx->resume_param , ret_code);
}
