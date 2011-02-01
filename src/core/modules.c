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
 *  2010-07-xx  created (bogdan)
 */

#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#include "mem/mem.h"
#include "version.h"
#include "modules.h"

/* path where the core modules are located */
static char* module_path = NULL;

/* all loaded module, disregarding the type */
static struct os_core_module* core_modules=NULL;



int set_load_module_path(char *s)
{
	module_path = s;
	return 0;
}


int load_core_module(char *module)
{
	#ifndef DLSYM_PREFIX
		/* define it to null */
		#define DLSYM_PREFIX
	#endif
	#define MODULES_DIR "src/core/modules"

	struct os_core_module *cm = NULL;
	struct os_core_module *it;;
	char *name = NULL;
	char *p;

	cm = (struct os_core_module*)shm_malloc(sizeof(struct os_core_module));
	if (cm==NULL) {
		LM_ERR("failed to allocate new module\n");
		goto error;
	}
	memset( cm, 0, sizeof(struct os_core_module));

	/* build lib full name */
	name = pkg_malloc
		( (module_path?strlen(module_path):(sizeof(INSTALL_DIR MODULES_DIR))) +
		1 + strlen(module) + 4 );
	if (name==NULL) {
		LM_ERR("no more pkg mem\n");
		goto error;
	}

	p = name;
	if (module_path) {
		memcpy( p, module_path, strlen(module_path) );
		p += strlen(module_path);
	} else {
		memcpy( p, INSTALL_DIR, sizeof(INSTALL_DIR)-1);
		p += sizeof(INSTALL_DIR)-1;
		*(p++) = '/';
		memcpy( p, MODULES_DIR, sizeof(MODULES_DIR)-1);
		p += sizeof(MODULES_DIR)-1;
	}
	*(p++) = '/';
	memcpy( p, module, strlen(module) );
	p += strlen(module);
	*(p++) = '.';
	*(p++) = 's';
	*(p++) = 'o';
	*(p++) =  0;

	/* load proto lib */
	cm->dl_handler = dlopen(name, RTLD_NOW); /* resolve all symbols now */
	if (cm->dl_handler==0){
		LM_ERR("could not open module lib <%s>: %s\n", name, dlerror() );
		goto error;
	}

	/* import interface */
	cm->interface = (struct core_module_interface*)dlsym(cm->dl_handler,
		DLSYM_PREFIX "interface");
	if ( (p =(char*)dlerror())!=0 ){
		LM_ERR("load_module: %s\n", p);
		goto error;
	}

	// TODO DLSYM_PREFIX in Makefile

	/* check version */
	if ( cm->interface->version==NULL || cm->interface->compile_flags==NULL) {
		LM_CRIT("BUG - version / compile flags not defined in proto <%s>\n",
			name );
		goto error;
	}
	if (strcmp(OPENSIPS_FULL_VERSION, cm->interface->version)!=0) {
		LM_ERR("version mismatch for %s; core: %s; module: %s\n",
			cm->interface->name, OPENSIPS_FULL_VERSION, cm->interface->version);
		goto error;
	}
	if (strcmp(OPENSIPS_COMPILE_FLAGS, cm->interface->compile_flags)!=0) {
		LM_ERR("protocol compile flags mismatch for %s "
			" \ncore: %s \nmodule: %s\n",
			cm->interface->name, OPENSIPS_COMPILE_FLAGS,
			cm->interface->compile_flags);
		goto error;
	}

	LM_DBG("module lib <%s> succesfully loaded\n", name );

	pkg_free(name);

	/* add module to the list */
	for( it=core_modules ; it && it->next ; it=it->next );
	if (it==NULL)
		core_modules = cm;
	else
		it->next = cm;

	/* load the module parameters */
	cm->param_section.name = cm->interface->name;
	cm->param_section.params = cm->interface->mod_params;
	global_append_section(  &cm->param_section );

	return 0;
error:
	if (name) pkg_free(name);
	if (cm) {
		if (cm->dl_handler) dlclose(cm->dl_handler);
		shm_free(cm);
	}
	return -1;
}


int init_all_core_module(void)
{
	struct os_core_module *cm;

	for( cm=core_modules ; cm ; cm=cm->next ) {
		if (cm->interface->mod_init && cm->interface->mod_init()<0) {
			LM_ERR("failed to init module <%s>\n",cm->interface->name);
			return -1;
		}
	}

	return 0;
}


void destroy_all_core_module(void)
{
	struct os_core_module *cm;
	struct os_core_module *next;

	for( cm=core_modules ; cm ; cm=next ) {
		next = cm->next;

		if (cm->interface->mod_destroy)
			cm->interface->mod_destroy();
		shm_free(cm);
	}
}

