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



#ifndef _CORE_MODULES_H
#define _CORE_MODULES_H

#include "config/params.h"
#include "version.h"
#include "mi/mi.h"

typedef enum {CORE_MODULE_LAYER, CORE_MODULE_UTILS} core_module_types;

typedef int  (*mod_init_f)(void);
typedef void (*mod_destroy_f)(void);
typedef int  (*mod_layerup_f)(void);
typedef int  (*mod_layerdown_f)(void);


struct core_module_interface {
	/* module definition */
	char *name;
	char *version;
	char *compile_flags;
	core_module_types type;
	unsigned int layer_no;
	/* parameters */
	config_param_t *mod_params;
	mi_funcs_t     *mod_mis;
	/* functions */
	mod_init_f      mod_init;
	mod_destroy_f   mod_destroy;
	mod_layerup_f   mod_layer_up;
	mod_layerdown_f mod_layer_down;
};

struct os_core_module {
	struct core_module_interface *interface;
	void                         *dl_handler;
	param_section_t               param_section;
	struct os_core_module        *next;
};


int set_load_module_path(char *s);

int load_core_module(char *name);

int init_all_core_module(void);

void destroy_all_core_module(void);

#endif

