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



%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "params.h"

#include "../log.h"

int cfg_errors;
extern int yylineno;
param_section_t* cur_section;

void yyerror(char*);
int yylex(void);
%}

%union { double dval; int ival; char * sval; }

%token <dval> DOUBLE
%token <ival> INT
%token <sval> STRING


%%
program:
        program section
        | section


section_def:
	'[' STRING ']'	{
			    cur_section = global_get_section($2);
			    if( cur_section == NULL )
			    {
				LM_WARN("Section:[%s] is not a section "
				    "defined by OpenSIPS\n",$2);
			    }
			}

section: section_def attributes
	| section_def

attributes: attributes attribute  |  attribute

attribute: STRING '=' INT    {
				if( set_int_param(cur_section,$1,$3) )
				{
				    LM_ERR("Setting parameter: %s = %d on line %d \n",
					$1,$3,yylineno);
				    cfg_errors++;
				}

				free($1);
			}
	|  STRING '=' DOUBLE {
				if( set_double_param(cur_section,$1,$3) )
				{
				    LM_ERR("Setting parameter: %s = %lf on line %d \n",
					$1,$3,yylineno);
				    cfg_errors++;
				}

				free($1);
			}

	|  STRING '=' STRING {
				if( set_string_param(cur_section,$1,$3) )
				{
				    LM_ERR("Setting parameter: %s = %s on line %d \n",
					$1,$3,yylineno);
				    cfg_errors++;
				}

				free($1);
			}



%%


void yyerror(char* x)
{
	fprintf(stderr,"%s",x);
	exit(-1);
}


