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



#include <time.h>


#include "resolve.h"
#include "dns_globals.h"

typedef struct _get_record_pack
{
	dns_get_record_answer * func;
	void * arg;
	struct rdata * answer;

} get_record_pack_t;


int get_record_dns_to_user(void * arg)
{
	get_record_pack_t * pack = (get_record_pack_t *) arg;
	pack->func(pack->arg, pack->answer);
	shm_free(arg);
	return 0;
}


void get_record_ares_to_dns(void *arg, int status, int timeouts,
				 unsigned char *abuf, int alen)
{
	get_record_pack_t * pack = (get_record_pack_t *) arg;

	if (status == ARES_SUCCESS)
	{
		pack->answer = parse_record(abuf, alen);

	} else
	{
		LM_ERR("Error in dns reply: %s\n", ares_strerror(status));
		pack->answer = NULL;
	}

	put_task_simple( reactor_in->disp, TASK_PRIO_RESUME_EXEC,
		get_record_dns_to_user, pack);
}


void get_record(char* name, int type, dns_get_record_answer func, void * param)
{
	get_record_pack_t * pack = (get_record_pack_t *) shm_malloc(sizeof (*pack));

	if( pack == NULL )
	{
		LM_ERR("Out of memory\n");
		goto error;
	}

	pack->func = func;
	pack->arg = param;

	lock_get(&ares_lock);
	ares_search(channel, name, ns_c_in, type, get_record_ares_to_dns, pack);
	lock_release(&ares_lock);

	return;

error:
	func(param, NULL);

}
