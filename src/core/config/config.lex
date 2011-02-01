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
 * TODO:
 * ------
 * error reporting
*/

%option yylineno
%{

#include <stdlib.h>
#include <stdio.h>
#include "config.tab.h"
#include "../log.h"

void yyerror(char*);
char * unescape( char * s,char c);
%}

letter [A-Za-z_\.:;]

%%

[ \t\n]+ ;

#[^\n]*\n ;

\+?-?[0-9]* {
            yylval.ival = atoi(yytext);
	    return INT;
	    }

(([0-9]+(\.[0-9]*)?)|([0-9]*\.[0-9]+)) {
            yylval.dval = atof(yytext);
	     return DOUBLE;
	    
	    }

{letter}({letter}|[0-9])*  {
            yylval.sval = strdup(yytext);
	    return STRING;
	    }

\"([^\"\\\n]|(\\\")|(\\\\)|(\\\n)|(\\\r)|(\\\t))*\" {
	    yylval.sval = unescape(yytext,'"');
	    return STRING;
	    }

\'([^\'\\\n]|(\\\')|(\\\\)|(\\\n)|(\\\r)|(\\\t))*\' {
	    yylval.sval = unescape(yytext,'\'');
	    return STRING;
	    }

[\[\]=]	    { return *yytext;}

.          {
	    char msg[100];
            sprintf(msg,"invalid character <%s> at line %d\n",yytext,yylineno);
            yyerror(msg);
	    }

%%
int yywrap(void) { return 1; }

char * unescape( char * s,char c)
{
    int len = strlen(s);
    int total = 0;
    char * ret = malloc(len+1);
    int i=0;

    if( ret == NULL )
    {
	LM_ERR("Unable to allocate memory\n");
	yyerror("Error unescaping string\n");
    }

    for( i=1;i<len-1;i++)
    {
	if( s[i] == '\\' )
	{
	    i++;
	    if( s[i] == '\\' )
		ret[total++] = '\\';
	    else if( s[i] == c)
		ret[total++] = c;
	    else if( s[i] == '\n')
		ret[total++] = '\n';
	    else if( s[i] == '\r')
		ret[total++] = '\r';
	    else if( s[i] == '\t')
		ret[total++] = '\t';

	}
	else
	{
	    ret[total++] = s[i];
	}
    }

    ret[total++] = 0;

    return ret;


}
