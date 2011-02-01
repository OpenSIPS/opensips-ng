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
 *  2010-01-xx  created (bogdan)
 */

#include <string.h>
#include <stdio.h>
#include <dlfcn.h>

#include "../mem/mem.h"
#include "../version.h"
#include "proto.h"

struct proto_info protos[PROTO_MAX];


void init_protos(void)
{
	unsigned short proto;

	memset( protos, 0, PROTO_MAX*sizeof(struct proto_info) );

	for ( proto=0 ; proto<PROTO_MAX ; proto++ ){
		protos[proto].proto = proto;
	}
}


void destroy_protos(void)
{
	unsigned short proto;

	for ( proto=0 ; proto<PROTO_MAX ; proto++ ){
		if (protos[proto].funcs.destroy)
			protos[proto].funcs.destroy();
	}
}


int load_proto_lib( unsigned short proto )
{
	#ifndef DLSYM_PREFIX
		/* define it to null */
		#define DLSYM_PREFIX
	#endif
	#define PROTO_DIR "src/core/net"

	struct proto_interface *pi;
	void* handle = NULL;
	char *path = NULL;
	char *p;

	LM_DBG("loading protocol %d\n",proto);

	/* build lib full name as "install_path/proto/proto.so" */
	path = pkg_malloc
		( sizeof(INSTALL_DIR PROTO_DIR) + 3 + 2*MAX_PROTO_STR_LEN + 4 );
	if (path==NULL) {
		LM_ERR("no more pkg mem\n");
		return -1;
	}

	p = path;
	memcpy( p, INSTALL_DIR, sizeof(INSTALL_DIR)-1);
	p += sizeof(INSTALL_DIR)-1;
	*(p++) = '/';
	memcpy( p, PROTO_DIR, sizeof(PROTO_DIR)-1);
	p += sizeof(PROTO_DIR)-1;
	*(p++) = '/';
	p = proto2str( proto, p);
	*(p++) = '/';
	p = proto2str( proto, p);
	*(p++) = '.';
	*(p++) = 's';
	*(p++) = 'o';
	*(p++) =  0;

	/* load proto lib */
	handle = dlopen(path, RTLD_NOW); /* resolve all symbols now */
	if (handle==0){
		LM_ERR("could not open proto lib <%s>: %s\n", path, dlerror() );
		goto error;
	}

	/* import interface */
	pi = (struct proto_interface*)dlsym(handle, DLSYM_PREFIX "interface");
	if ( (p =(char*)dlerror())!=0 ){
		LM_ERR("load_module: %s\n", p);
		goto error;
	}

	// TODO DLSYM_PREFIX in Makefile

	/* check version */
	if ( pi->version==NULL || pi->compile_flags==NULL) {
		LM_CRIT("BUG - version / compile flags not defined in proto <%s>\n",
			path );
		goto error;
	}
	if (strcmp(OPENSIPS_FULL_VERSION, pi->version)!=0) {
		LM_ERR("version mismatch for %s; core: %s; proto: %s\n",
			pi->name, OPENSIPS_FULL_VERSION, pi->version );
		goto error;
	}
	if (strcmp(OPENSIPS_COMPILE_FLAGS, pi->compile_flags)!=0) {
		LM_ERR("protocol compile flags mismatch for %s "
			" \ncore: %s \nprotocol: %s\n",
			pi->name, OPENSIPS_COMPILE_FLAGS, pi->compile_flags);
		goto error;
	}

	LM_DBG("proto lib <%s> succesfully loaded\n", path );

	protos[proto].funcs = pi->funcs;

	if (protos[proto].funcs.init && protos[proto].funcs.init()!=0) {
		LM_ERR("failed to init protocol %d\n",proto);
		goto error;
	}

	pkg_free(path);

	return 0;
error:
	if (path) pkg_free(path);
	if (handle) dlclose(handle);
	return -1;
}


