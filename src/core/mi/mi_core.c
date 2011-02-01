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
 *  2010-03-28  addepted to 2.0 (bogdan)
 */


/*!
 * \file 
 * \brief MI :: Core 
 * \ingroup mi
 */



#include <time.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include "../log.h"
#include "../config.h"
#include "../globals.h"
#include "../utils.h"
#include "../threading.h"
#include "mi.h"


static char  up_time_buf[27];
static str   up_since_ctime;

static int init_mi_uptime(void)
{
	ctime_r( &startup_time, up_time_buf);
	up_since_ctime.len = strlen(up_time_buf)-1;
	up_since_ctime.s = up_time_buf;
	return 0;
}


static struct mi_root *mi_uptime(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	time_t now;
	char   *p;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return NULL;
	rpl = &rpl_tree->node;

	time(&now);
	p = ctime(&now);
	node = add_mi_node_child( rpl, MI_DUP_VALUE, MI_SSTR("Now"),
		p, strlen(p)-1);
	if (node==0)
		goto error;

	node = add_mi_node_child( rpl, 0, MI_SSTR("Up since"),
		up_since_ctime.s, up_since_ctime.len);
	if (node==0)
		goto error;

	node = addf_mi_node_child( rpl, 0, MI_SSTR("Up time"),
		"%lu [sec]", (unsigned long)difftime(now, startup_time) );
	if (node==0)
		goto error;

	return rpl_tree;
error:
	LM_ERR("failed to add node\n");
	free_mi_tree(rpl_tree);
	return NULL;
}



static struct mi_root *mi_version(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return NULL;
	rpl = &rpl_tree->node;

	node = add_mi_node_child( rpl, 0, MI_SSTR("Server"), SERVER_HDR+8,
		SERVER_HDR_LEN-8);
	if (node==0) {
		LM_ERR("failed to add node\n");
		free_mi_tree(rpl_tree);
		return NULL;
	}

	return rpl_tree;
}



static struct mi_root *mi_pwd(struct mi_root *cmd, void *param)
{
	int max_len = 0;
	char *cwd_buf = 0;
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;

	max_len = pathmax();
	cwd_buf = pkg_malloc(max_len);
	if (cwd_buf==NULL) {
		LM_ERR("no more pkg mem\n");
		return NULL;
	}

	if (getcwd(cwd_buf, max_len)==0) {
		LM_ERR("getcwd failed = %s\n",strerror(errno));
		pkg_free(cwd_buf);
		return NULL;
	}

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0) {
		pkg_free(cwd_buf);
		return NULL;
	}
	rpl = &rpl_tree->node;

	node = add_mi_node_child( rpl, MI_FREE_VALUE, MI_SSTR("WD"), cwd_buf,strlen(cwd_buf));
	if (node==0) {
		LM_ERR("failed to add node\n");
		pkg_free(cwd_buf);
		free_mi_tree(rpl_tree);
		return NULL;
	}

	return rpl_tree;
}



static struct mi_root *mi_arg(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	int n;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	for ( n=0; n<osips_argc ; n++ ) {
		node = add_mi_node_child(rpl, 0, 0, 0,
			osips_argv[n], strlen(osips_argv[n]));
		if (node==0) {
			LM_ERR("failed to add node\n");
			free_mi_tree(rpl_tree);
			return 0;
		}
	}

	return rpl_tree;
}



static struct mi_root *mi_which(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_cmd  *cmds;
	struct mi_node *rpl;
	struct mi_node *node;
	int size;
	int i;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	get_mi_cmds( &cmds, &size);
	for ( i=0 ; i<size ; i++ ) {
		node = add_mi_node_child( rpl, 0, 0, 0, cmds[i].name.s,
			cmds[i].name.len);
		if (node==0) {
			LM_ERR("failed to add node\n");
			free_mi_tree(rpl_tree);
			return 0;
		}
	}

	return rpl_tree;
}



static struct mi_root *mi_ps(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *rpl;
	struct mi_node *node;
	struct mi_attr *attr;
	char *p;
	int len;
	int i,threads_no;
	struct thread_info *threads_table;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;

	get_threads_info( &threads_table, &threads_no);

	for ( i=0 ; i<threads_no ; i++ ) {
		node = add_mi_node_child(rpl, 0, MI_SSTR("Thread"), 0, 0 );
		if (node==0)
			goto error;

		p = int2str((unsigned long)i, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, MI_SSTR("ID"), p, len);
		if (attr==0)
			goto error;

		p = int2str((unsigned long)threads_table[i].id, &len);
		attr = add_mi_attr( node, MI_DUP_VALUE, MI_SSTR("TID"), p, len);
		if (attr==0)
			goto error;

		attr = add_mi_attr( node, 0, MI_SSTR("Type"),
			threads_table[i].name, strlen(threads_table[i].name));
		if (attr==0)
			goto error;
	}

	return rpl_tree;
error:
	LM_ERR("failed to add node\n");
	free_mi_tree(rpl_tree);
	return 0;
}



static struct mi_root *mi_kill(struct mi_root *cmd, void *param)
{
	kill(0, SIGTERM);

	return 0;
}



static struct mi_root *mi_debug(struct mi_root *cmd, void *param)
{
	struct mi_root *rpl_tree;
	struct mi_node *node;
	char *p;
	int len;
	int new_debug;

	node = cmd->node.kids;
	if (node!=NULL) {
		if (str2sint( &node->value, &new_debug) < 0)
			return init_mi_tree( 400, MI_SSTR(MI_BAD_PARM));
	} else
		new_debug = debug;

	rpl_tree = init_mi_tree( 200, MI_SSTR(MI_OK));
	if (rpl_tree==0)
		return 0;

	p = sint2str((long)new_debug, &len);
	node = add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE,
		MI_SSTR("DEBUG"),p, len);
	if (node==0) {
		free_mi_tree(rpl_tree);
		return 0;
	}

	debug = new_debug;

	return rpl_tree;
}



static mi_funcs_t mi_core_cmds[] = {
	{ "uptime",      mi_uptime,     MI_NO_INPUT_FLAG,  0,  init_mi_uptime },
	{ "version",     mi_version,    MI_NO_INPUT_FLAG,  0,  0 },
	{ "pwd",         mi_pwd,        MI_NO_INPUT_FLAG,  0,  0 },
	{ "arg",         mi_arg,        MI_NO_INPUT_FLAG,  0,  0 },
	{ "which",       mi_which,      MI_NO_INPUT_FLAG,  0,  0 },
	{ "ps",          mi_ps,         MI_NO_INPUT_FLAG,  0,  0 },
	{ "kill",        mi_kill,       MI_NO_INPUT_FLAG,  0,  0 },
	{ "debug",       mi_debug,                     0,  0,  0 },
	{ 0, 0, 0, 0, 0}
};



int init_mi_core(void)
{
	int i;

	if (register_mi_mod( "core", mi_core_cmds)<0) {
		LM_ERR("unable to register core MI cmds\n");
		return -1;
	}

	/* FIXME - init should't be done here
	 * do MI commands init with all exported MI commands from modules
	 */
	for (i=0;mi_core_cmds[i].name;i++)
	{
		if (mi_core_cmds[i].init_f && mi_core_cmds[i].init_f() != 0)
		{
			LM_ERR("failed to init command %s\n",mi_core_cmds[i].name);
		}
	}


	return 0;
}
