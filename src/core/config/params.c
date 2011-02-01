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
 *  2010-01-xx  created (adragus)
 *
*/


#include <string.h>
#include <stdio.h>

#include "../log.h"
#include "../mem/mem.h"
#include "params.h"

param_list_t * sections;

void append_section(param_list_t ** list, param_section_t* section)
{
	param_list_t * node = shm_malloc(sizeof ( param_list_t));

	if( node == NULL )
	{
		LM_ERR("Error in allocating memory\n");
		return;
	}

	node->section = section;
	node->next = *list;
	*list = node;
}

param_section_t* get_section(param_list_t * list, char* name)
{
	param_section_t* ret_value = NULL;

	while (list)
	{
		if (strcmp(list->section->name, name) == 0)
			ret_value = list->section;
		list = list->next;
	}

	return ret_value;
}

config_param_t* get_param(param_section_t* section, char * name)
{

	int i = 0;
	config_param_t * ret_value = NULL;

	while (section->params[i].name != 0)
	{
		if (strcmp(section->params[i].name, name) == 0)
			ret_value = &section->params[i];
		i++;
	}


	if (ret_value == NULL)
	{
		LM_WARN("Parameter:[%s] does not exist in section: [%s]\n",
			name, section->name);
	}
	return ret_value;
}


void global_append_section(param_section_t* section)
{
	append_section(&sections, section);
}


void global_destroy_sections(void)
{
	param_list_t *next;
	while ( sections ) {
		next = sections->next;
		shm_free(sections);
		sections = next;
	}
}


param_section_t* global_get_section(char* name)
{
	return get_section(sections, name);
}


int set_int_param(param_section_t* section, char * name, int val)
{

	if (section == NULL)
		return -1;

	config_param_t * p = get_param(section, name);

	if (p == NULL)
		return -1;


	if ((p->flags & PARAM_TYPE_INT) == 0)
	{
		LM_ERR("Parameter:[%s] in section: [%s] does not want int value\n",
		name, section->name);
		return -1;
	}

	if (p->flags & PARAM_TYPE_FUNC)
	{
		return  ((param_int_func)p->dest)(val);
	}


	*(int*) p->dest = val;

	if (p->verify)
	{
		return p->verify(p);
	}

	return 0;
}

int set_double_param(param_section_t* section, char * name, double val)
{
	if (section == NULL)
		return -1;

	config_param_t * p = get_param(section, name);

	if (p == NULL)
		return -1;


	if ((p->flags & PARAM_TYPE_DOUBLE) == 0)
	{
		LM_ERR("Parameter:[%s] in section: [%s] does not want double value\n",
		name, section->name);
		return -1;
	}


	if (p->flags & PARAM_TYPE_FUNC)
	{
		return  ((param_double_func)p->dest)(val);
	}


	*(double*) p->dest = val;

	if (p->verify)
	{
		return p->verify(p);
	}

	return 0;

}

int set_string_param(param_section_t* section, char * name, char * val)
{
	if (section == NULL)
		return -1;

	config_param_t * p = get_param(section, name);

	if (p == NULL)
		return -1;


	if ((p->flags & PARAM_TYPE_STR) == 0 && (p->flags & PARAM_TYPE_STRING) == 0)
	{
		LM_ERR("Parameter:[%s] in section: [%s] does not want string value\n",
		name, section->name);
		return -1;
	}


	if (p->flags & PARAM_TYPE_FUNC)
	{
		return  ((param_string_func)p->dest)(val);
	}


	if (p->flags & PARAM_TYPE_STRING)
		*(char**) p->dest = val;
	else
	{
		((str*) p->dest)->s = val;
		((str*) p->dest)->len = strlen(((str*) p->dest)->s);
	}


	if (p->verify)
	{
		return p->verify(p);
	}

	return 0;
}
