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
 * History:
 * ---------
 *  2010-03-28  added (bogdan)
 */

/*!
 * \file 
 * \brief MI :: Attributes
 * \ingroup mi
 */

#include <string.h>

#include "../log.h"
#include "../mem/mem.h"
#include "mi.h"

static struct mi_cmd*  mi_cmds = 0;
static int mi_cmds_no = 0;

mi_flush_f *crt_flush_fnct = NULL;
void *crt_flush_param = NULL;


static inline int get_mi_id( char *name, int len)
{
	int n;
	int i;

	for( n=0,i=0 ; i<len ; n+=name[i] ,i++ );
	return n;
}


static inline struct mi_cmd* lookup_mi_cmd_id(int id,char *name, int len)
{
	int i;

	for( i=0 ; i<mi_cmds_no ; i++ ) {
		if ( id==mi_cmds[i].id && len==mi_cmds[i].name.len &&
		memcmp(mi_cmds[i].name.s,name,len)==0 )
			return &mi_cmds[i];
	}

	return 0;
}


int register_mi_mod( char *mod_name, mi_funcs_t *mis)
{
	int ret;
	int i;

	if (mis==0)
		return 0;

	for ( i=0 ; mis[i].name ; i++ ) {
		ret = register_mi_cmd( mis[i].cmd, mis[i].name, mis[i].param,
			mis[i].init_f, mis[i].flags);
		if (ret!=0) {
			LM_ERR("failed to register cmd <%s> for module %s\n",
					mis[i].name,mod_name);
		}
	}
	return 0;
}



int register_mi_cmd( mi_cmd_f f, char *name, void *param,
									mi_child_init_f in, unsigned int flags)
{
	struct mi_cmd *cmds;
	int id;
	int len;

	if (f==0 || name==0) {
		LM_ERR("invalid params f=%p, name=%s\n", f, name);
		return -1;
	}

	len = strlen(name);
	id = get_mi_id(name,len);

	if (lookup_mi_cmd_id( id, name, len)) {
		LM_ERR("command <%.*s> already registered\n", len, name);
		return -1;
	}

	cmds = (struct mi_cmd*)shm_realloc( mi_cmds,
			(mi_cmds_no+1)*sizeof(struct mi_cmd) );
	if (cmds==0) {
		LM_ERR("no more shm memory\n");
		return -1;
	}

	mi_cmds = cmds;
	mi_cmds_no++;

	cmds = &cmds[mi_cmds_no-1];

	cmds->f = f;
	cmds->init_f = in;
	cmds->flags = flags;
	cmds->name.s = name;
	cmds->name.len = len;
	cmds->id = id;
	cmds->param = param;

	return 0;
}


struct mi_cmd* lookup_mi_cmd( char *name, int len)
{
	int id;

	id = get_mi_id(name,len);
	return lookup_mi_cmd_id( id, name, len);
}


void get_mi_cmds( struct mi_cmd** cmds, int *size)
{
	*cmds = mi_cmds;
	*size = mi_cmds_no;
}


void destroy_mi_cmds(void)
{
	if (mi_cmds)
		shm_free(mi_cmds);
}

