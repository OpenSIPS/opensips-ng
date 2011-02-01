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


#include "../str.h"

#ifndef _OPENSIPS_PARAMS_H
#define _OPENSIPS_PARAMS_H

#define PARAM_TYPE_INT	    (1<<0)
#define PARAM_TYPE_DOUBLE   (1<<1)
#define PARAM_TYPE_STR	    (1<<2)
#define PARAM_TYPE_STRING   (1<<3)
#define PARAM_TYPE_FUNC	    (1<<7)


/*
 * Structure that describes a parameter
 *
 */
typedef struct config_param
{
    char* name;
    /* Destination where the information will be put.
     * May be param_*_func in which case it will not be set,
     * it will be called with the corresponding value
     * */
    void * dest;
    /*
     * Types of data accepted.
     * It is a mask of PARAM_TYPE_* values
     */
    int flags;
    /*
     * Function to verify a parameter.
     * May be NULL.
     */
    int (*verify) (struct config_param *);
} config_param_t;

/* type of function used to verify parameters*/
typedef int (*param_func)(config_param_t *);

/* type of function called if you don't wish to overwrite parameters */
typedef int (*param_int_func)( int );
typedef int (*param_double_func)( double );
typedef int (*param_string_func)( char* );


typedef struct param_section
{
    char* name;
    /* zero terminated array of parameters */
    config_param_t * params;

} param_section_t;

typedef struct param_list
{
    param_section_t* section;
    struct param_list * next;

} param_list_t;


/*
 *  Function that adds sections of parameters to the global list
 */
void global_append_section(param_section_t* section);


void global_destroy_sections(void);


/*
 *  Function that gets sections of parameters from the global list
 */
param_section_t* global_get_section(char* name);


int set_int_param(param_section_t* section, char * name, int val);
int set_double_param(param_section_t* section, char * name, double val);
int set_string_param(param_section_t* section, char * name, char * val);

/*
 *  Functions that are not on global values
 */
void append_section(param_list_t ** list, param_section_t* section);
param_section_t* get_section(param_list_t * list, char* name);
config_param_t* get_param(param_section_t* section, char* name);

#endif

