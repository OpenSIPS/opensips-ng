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

typedef struct _resolve_pack
{
	dns_resolve_answer * func;
	void * arg;
	struct hostent* answer;
	
	char * name;
	int type;

} resolve_pack_t;

int resolve_dns_to_user(void * arg)
{
	resolve_pack_t * pack = (resolve_pack_t *) arg;
	pack->func(pack->arg, pack->answer);
	shm_free(arg);
	return 0;
}

void resolve_ares_to_dns(void *arg, int status, int timeouts,
						 struct hostent * he)
{
	//TODO speed improvement if we skip the first query through the dispatcher
	resolve_pack_t * pack = (resolve_pack_t *) arg;

	if (status == ARES_SUCCESS)
	{
		pack->answer = hostent_cpy(he);
	} else
	{
		if( dns_try_ipv6 && pack->type == AF_INET)
		{
			LM_DBG("Trying IPv6 query for %s \n", pack->name);
			pack->type = AF_INET6;
			ares_gethostbyname(channel, pack->name, pack->type,
										resolve_ares_to_dns, pack);
			return;
		}
		else
		{
			LM_ERR("Error in dns reply: %s\n", ares_strerror(status));
			pack->answer = NULL;
		}
	}

	put_task_simple(reactor_in->disp, TASK_PRIO_RESUME_EXEC,
					resolve_dns_to_user, pack);

}

void resolvehost(char * name, dns_resolve_answer func, void * param)
{
	resolve_pack_t * pack = (resolve_pack_t *) shm_malloc(sizeof (*pack));

	if (pack == NULL)
	{
		LM_ERR("Out of memory\n");
		goto error;
	}

	pack->func = func;
	pack->arg = param;
	pack->name = name;
	pack->type = AF_INET;

	lock_get(&ares_lock);
	ares_gethostbyname(channel, pack->name, pack->type,
						resolve_ares_to_dns, pack);
	lock_release(&ares_lock);

	return;

error:
	func(param, NULL);

}

