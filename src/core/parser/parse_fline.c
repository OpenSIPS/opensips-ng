/*
 * $Id: parse_fline.c 5891 2009-07-20 12:53:09Z bogdan_iancu $
 * 
 * sip first line parsing automaton
 * 
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2020 OpenSIPS Project
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */


#include "../log.h"
#include "msg_parser.h"
#include "parser_f.h"
#include "parse_methods.h"
#include "../mem/mem.h"
#include "../utils.h"

/* grammar:
	request  =  method SP uri SP version CRLF
	response =  version SP status  SP reason  CRLF
	(version = "SIP/2.0")
*/

enum { START,
       INVITE1, INVITE2, INVITE3, INVITE4, INVITE5,
       ACK1, ACK2,
       CANCEL1, CANCEL2, CANCEL3, CANCEL4, CANCEL5,
       BYE1, BYE2,
       SIP1, SIP2, SIP3, SIP4, SIP5, SIP6,
       FIN_INVITE = 100, FIN_ACK, FIN_CANCEL, FIN_BYE, FIN_SIP,
       P_METHOD = 200, L_URI, P_URI, L_VER, 
       VER1, VER2, VER3, VER4, VER5, VER6, FIN_VER,
       L_STATUS, P_STATUS, L_REASON, P_REASON,
       L_LF, F_CR, F_LF
};


#ifdef _CURRENTLY_UNUSED
char* parse_fline(char* buffer, char* end, struct msg_start* fl)
{
	char* tmp;
	register int state;
	unsigned short stat;

	stat=0;
	fl->type=SIP_REQUEST;
	state=START;
	for(tmp=buffer;tmp<end;tmp++){
		switch(*tmp){
			case ' ':
			case '\t':
				switch(state){
					case START: /*allow space at the begining, although not
								  legal*/
						break;
					case L_URI:
					case L_VER:
					case L_STATUS:
					case L_REASON:
					case L_LF:
						 /*eat  space*/
						break;
					case FIN_INVITE:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_INVITE;
						state=L_URI;
						break;
					case FIN_ACK:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_ACK;
						state=L_URI;
						break;
					case FIN_CANCEL:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_CANCEL;
						state=L_URI;
						break;
					case FIN_BYE:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_BYE;
						state=L_URI;
						break;
					case FIN_SIP:
						*tmp=0;
						fl->u.reply.version.len=tmp-fl->u.reply.version.s;
						state=L_STATUS;
						fl->type=SIP_REPLY;
						break;
					case P_URI:
						*tmp=0;
						fl->u.request.uri.len=tmp-fl->u.request.uri.s;
						state=L_VER;
						break;
					case FIN_VER:
						*tmp=0;
						fl->u.request.version.len=tmp-fl->u.request.version.s;
						state=L_LF;
						break;
					case P_STATUS:
						*tmp=0;
						fl->u.reply.status.len=tmp-fl->u.reply.status.s;
						state=L_REASON;
						break;
					case P_REASON:
					 /*	*tmp=0;
						fl->u.reply.reason.len=tmp-fl->u.reply.reason.s;
						*/
						break;
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
						LM_ERR("invalid version in request\n");
						goto error;
					case P_METHOD:
					default:
						*tmp=0;
						fl->u.request.method.len=tmp-fl->u.request.method.s;
						fl->u.request.method_value=METHOD_OTHER;
						state=L_URI;
				}
				break;
			case 's':
			case 'S':
				switch(state){
					case START:
						state=SIP1;
						fl->u.reply.version.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number character <%c> in request"
								"status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
						fl->u.request.version.s=tmp;
						state=VER1;
						break;
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
					
			case 'i':
			case 'I':
				switch(state){
					case START:
						state=INVITE1;
						fl->u.request.method.s=tmp;
						break;
					case INVITE3:
						state=INVITE4;
						break;
					case SIP1:
						state=SIP2;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER1:
						state=VER2;
						break;
					case L_VER:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
				
			case 'p':
			case 'P':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP2:
						state=SIP3;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER2:
						state=VER3;
						break;
					case L_VER:
					case VER1:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			
			case '/':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP3:
						state=SIP4;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER3:
						state=VER4;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case '2':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP4:
						state=SIP5;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
						stat=stat*10+*tmp-'0';
						break;
					case L_STATUS:
						stat=*tmp-'0';
						fl->u.reply.status.s=tmp;
						break;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER4:
						state=VER5;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case '.':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP5:
						state=SIP6;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER5:
						state=VER6;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case '0':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case SIP6:
						state=FIN_SIP;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
						stat=stat*10;
						break;
					case L_STATUS:
						stat=0;
						fl->u.reply.status.s=tmp;
						break;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case VER6:
						state=FIN_VER;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case FIN_VER:
						LM_ERR("invalid version "
								" in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;
			
			case 'n':
			case 'N':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE1:
						state=INVITE2;
						break;
					case CANCEL2:
						state=CANCEL3;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'v':
			case 'V':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE2:
						state=INVITE3;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 't':
			case 'T':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE4:
						state=INVITE5;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'e':
			case 'E':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case INVITE5:
						state=FIN_INVITE;
						break;
					case CANCEL4:
						state=CANCEL5;
						break;
					case BYE2:
						state=FIN_BYE;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'a':
			case 'A':
				switch(state){
					case START:
						state=ACK1;
						fl->u.request.method.s=tmp;
						break;
					case CANCEL1:
						state=CANCEL2;
						break;
					case BYE2:
						state=FIN_BYE;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'c':
			case 'C':
				switch(state){
					case START:
						state=CANCEL1;
						fl->u.request.method.s=tmp;
						break;
					case CANCEL3:
						state=CANCEL4;
						break;
					case ACK1:
						state=ACK2;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'k':
			case 'K':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case ACK2:
						state=FIN_ACK;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'l':
			case 'L':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case CANCEL5:
						state=FIN_CANCEL;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'b':
			case 'B':
				switch(state){
					case START:
						state=BYE1;
						fl->u.request.method.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case 'y':
			case 'Y':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case BYE1:
						state=BYE2;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid "
								"character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
				break;

			case '\r':
				switch(state){
					case P_REASON:
						*tmp=0;
						fl->u.reply.reason.len=tmp-fl->u.reply.reason.s;
						state=F_CR;
						break;
					case L_LF:
						state=F_CR;
						break;
					case FIN_VER:
						*tmp=0;
						fl->u.request.version.len=tmp-fl->u.request.version.s;
						state=F_CR;
						break;
					case L_REASON:
						state=F_CR;
						break;
					default:
						LM_ERR("invalid message\n");
						goto error;
				}
				break;

			case '\n':
				switch(state){
					case P_REASON:
						*tmp=0;
						fl->u.reply.reason.len=tmp-fl->u.reply.reason.s;
						state=F_LF;
						goto skip;
					case FIN_VER:
						*tmp=0;
						fl->u.request.version.len=tmp-fl->u.request.version.s;
						state=F_LF;
						goto skip;
					case L_REASON:
					case L_LF:
					case F_CR:
						state=F_LF;
						goto skip;
					default:
						LM_ERR("invalid message\n");
						goto error;
				}
				break;

			case '1':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
						stat=stat*10+*tmp-'0';
						break;
					case L_STATUS:
						stat=*tmp-'0';
						state=P_STATUS;
						fl->u.reply.status.s=tmp;
						break;
					case L_LF:
						LM_ERR("invalid character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
						
			default:
				switch(state){
					case START:
						state=P_METHOD;
						fl->u.request.method.s=tmp;
						break;
					case P_URI:
					case P_REASON:
					case P_METHOD:
						break;
					case L_REASON:
						fl->u.reply.reason.s=tmp;
						state=P_REASON;
						break;
					case P_STATUS:
					case L_STATUS:
						LM_ERR("non-number "
								"character <%c> in request status\n", *tmp);
						goto error;
					case L_LF:
						LM_ERR("invalid character <%c> in request\n", *tmp);
						goto error;
					case L_URI:
						fl->u.request.uri.s=tmp;
						state=P_URI;
						break;
					case L_VER:
					case VER1:
					case VER2:
					case VER3:
					case VER4:
					case VER5:
					case VER6:
					case FIN_VER:
						LM_ERR("invalid version in request\n");
						goto error;
					default:
						state=P_METHOD;
				}
		}
	}
skip:
	fl->len=tmp-buf;
	if (fl->type==SIP_REPLY){
		fl->u.reply.statuscode=stat;
		/* fl->u.reply.statusclass=stat/100; */
	}
	return tmp;
	
error:
	LM_ERR("while parsing first line (state=%d)\n", state);
	fl->type=SIP_INVALID;
	return tmp;
}

#endif /* currently unused */

/* parses the first line, returns pointer to  next line  & fills fl;
   also  modifies buffer (to avoid extra copy ops) */
char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl)
{
	
	char *tmp;
	char* second;
	char* third;
	char* nl;
	char* end;
	char s1,s2,s3;

	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status  SP reason  CRLF
		(version = "SIP/2.0")
	*/
	end=buffer+len;

	/* minimum size ? */
	if (len <=16 ) {
		LM_DBG("message too short: %d\n", len);
		goto too_short;
	}

	tmp=buffer;
  	/* is it perhaps a reply, ie does it start with "SIP...." ? */
	if ( (*tmp=='S' || *tmp=='s') && 
	strncasecmp( tmp+1, SIP_VERSION+1, SIP_VERSION_LEN-1)==0 &&
	(*(tmp+SIP_VERSION_LEN)==' ')) {
		fl->type=SIP_REPLY;
		fl->u.reply.version.len=SIP_VERSION_LEN;
		tmp=buffer+SIP_VERSION_LEN;
	} else IFISMETHOD( INVITE, 'I' )
	else IFISMETHOD( CANCEL, 'C')
	else IFISMETHOD( ACK, 'A' )
	else IFISMETHOD( BYE, 'B' ) 
	else IFISMETHOD( INFO, 'I' )
	/* if you want to add another method XXX, include METHOD_XXX in
           H-file (this is the value which you will take later in
           processing and define XXX_LEN as length of method name;
	   then just call IFISMETHOD( XXX, 'X' ) ... 'X' is the first
	   latter; everything must be capitals
	*/
	else {
		/* neither reply, nor any of known method requests, 
		   let's believe it is an unknown method request
        */
		fl->type=SIP_REQUEST;
		tmp=eat_token_end(buffer,buffer+len);
		if (tmp==buffer){
			LM_INFO("empty first line\n");
			goto error1;
		}
		if (tmp>=end){
			LM_INFO("first token not finished\n");
			goto too_short;
		}
		if (*tmp!=' ') {
			LM_INFO("method not followed by SP\n");
			goto error1;
		}
		/* see if it is another known method */
		/* fl->u.request.method_value=METHOD_OTHER; */
		if(parse_method(buffer, tmp,
		(unsigned int*)&fl->u.request.method_value)==0)
		{
			LM_INFO("failed to parse the method\n");
			goto error1;
		}
		fl->u.request.method.len=tmp-buffer;
	}
	

	/* identifying type of message over now; 
	   tmp points at space after; go ahead */

	fl->u.request.method.s=buffer;  /* store ptr to first token */
	second=tmp+1;			/* jump to second token */

	/* next element */
	tmp=eat_token_end(second, end);
	if (tmp>=end)
		goto too_short;
	third=eat_space_end(tmp, end);

	if (third==tmp)
		goto error;
	if (third >=end)
		goto too_short;
	fl->u.request.uri.s=second;
	fl->u.request.uri.len=tmp-second;

	if (fl->type==SIP_REPLY) {
		/* if reply, validate the status code */
		if (fl->u.reply.status.len!=3) {
			LM_INFO("len(status code)!=3: %.*s\n",
				fl->u.request.uri.len, ZSW(second) );
			goto error;
		}
		s1=*second; s2=*(second+1);s3=*(second+2);
		if (s1>='0' && s1<='9' && 
		    s2>='0' && s2<='9' &&
		    s3>='0' && s3<='9' ) {
			fl->u.reply.statuscode=(s1-'0')*100+10*(s2-'0')+(s3-'0');
		} else {
			LM_INFO("status_code non-numerical: %.*s\n",
				fl->u.request.uri.len, ZSW(second) );
			goto error;
		}

	 	/*  what is next (reason phrase) may contain almost anything,
 		 *  including spaces, so we don't care about it */

		/* find end of line ('\n' or '\r') */
		tmp = eat_token2_end(third,end,'\r'); 
		if (tmp>=end) 
			goto too_short;
		nl = tmp;
	} else {
		/* if request, version must follow */
		tmp=eat_token_end(third,end);
		if (tmp==third)
			goto error;
		if (tmp>=end)
			goto too_short;
		nl=eat_space_end(tmp,end);
		if (nl>=end)
			goto too_short;
	}

	/* tmp points at the end of the third token, while
	 * nl point (if msg correct) at EOH (\n or \r) */
	if (*nl=='\r') {
		nl++;
		if( nl >= end )
			goto too_short;
		if ( *nl=='\n')
			nl++;
	} else if (*nl=='\n') {
		nl++;
	} else 
		goto error;

	fl->u.request.version.s=third;
	fl->u.request.version.len=tmp-third;
	fl->len=nl-buffer;

	return nl;

error:
	LM_ERR("bad %s first line around char %d \n",
		(fl->type==SIP_REPLY)?"reply(status)":"request",
		(int)(long)(tmp-buffer) );
	LM_ERR("parsed so far: %.*s\n", (int)(long)(tmp-buffer), buffer );
error1:
	fl->type=SIP_INVALID;
	LM_DBG("bad message\n");
	return tmp;
too_short:
	return NULL;
}
