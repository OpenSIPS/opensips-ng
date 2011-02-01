/*
 * Copyright (C) 2001-2003 FhG Fokus
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


#ifndef _H_CORE_UTILS
#define _H_CORE_UTILS

#include <sys/time.h>
#include <sys/select.h>
#include "str.h"
#include "log.h"


/* zero-string wrapper */
#define ZSW(_c) ((_c)?(_c):"")

#define trim_r( _mystr ) \
	do{	static char _c; \
		while( ((_mystr).len) && ( ((_c=(_mystr).s[(_mystr).len-1]))==0 ||\
									_c=='\r' || _c=='\n' ) \
				) \
			(_mystr).len--; \
	}while(0)


#define trim_len( _len, _begin, _mystr ) \
	do{ 	static char _c; \
		(_len)=(_mystr).len; \
		while ((_len) && ((_c=(_mystr).s[(_len)-1])==0 || _c=='\r' || \
					_c=='\n' || _c==' ' || _c=='\t' )) \
			(_len)--; \
		(_begin)=(_mystr).s; \
		while ((_len) && ((_c=*(_begin))==' ' || _c=='\t')) { \
			(_len)--;\
			(_begin)++; \
		} \
	}while(0)

/* char to hex conversion table */
static char fourbits2char[16] = { '0', '1', '2', '3', '4', '5',
	'6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };


/*
 * Convert a str into integer
 */
static inline int str2int(str* s, unsigned int* n)
{
	int i;

	*n = 0;
	for( i=0; i<s->len ; i++ ) {
		if ( (s->s[i]>='0') && (s->s[i]<='9') ) {
			*n = (*n)*10 + s->s[i] - '0';
		} else {
			return -1;
		}
	}
	return 0;
}


static inline int btostr( char *p,  unsigned char val)
{
	unsigned int a,b,i =0;

	if ( (a=val/100)!=0 )
		*(p+(i++)) = a+'0';         /*first digit*/
	if ( (b=val%100/10)!=0 || a)
		*(p+(i++)) = b+'0';        /*second digit*/
	*(p+(i++)) = '0'+val%10;              /*third digit*/

	return i;
}


/* 2^64~= 16*10^18 => 19+1+1 sign + digits + \0 */
#define INT2STR_MAX_LEN  (1+19+1+1)

/* INTeger-TO-Buffer-STRing : convers an unsigned long to a string 
 * IMPORTANT: the provided buffer must be at least INT2STR_MAX_LEN size !! */
static inline char* int2bstr(unsigned long l, char *s, int* len)
{
	int i;

	i=INT2STR_MAX_LEN-2;
	s[INT2STR_MAX_LEN-1]=0; /* null terminate */
	do{
		s[i]=l%10+'0';
		i--;
		l/=10;
	}while(l && (i>=0));
	if (l && (i<0)){
		LM_CRIT("overflow error\n");
	}
	if (len) *len=(INT2STR_MAX_LEN-2)-i;
	return &s[i+1];
}


/* INTeger-TO-STRing : convers an unsigned long to a string 
 * returns a pointer to a static buffer containing l in asciiz & sets len */
extern char int2str_buf[INT2STR_MAX_LEN];
static inline char* int2str(unsigned long l, int* len)
{
	return int2bstr( l, int2str_buf, len);
}


/* Signed INTeger-TO-STRing: convers a long to a string
 * returns a pointer to a static buffer containing l in asciiz & sets len */
static inline char* sint2str(long l, int* len)
{
	int sign;
	char *p;

	sign = 0;
	if(l<0) {
		sign = 1;
		l = -l;
	}
	p = int2str((unsigned long)l, len);
	if(sign) {
		*(--p) = '-';
		if (len) (*len)++;
	}
	return p;
}

/* double output length assumed ; does NOT zero-terminate */
inline static int string2hex(
	/* input */ unsigned char *str, int len,
	/* output */ char *hex )
{
	int orig_len;

	if (len==0) {
		*hex='0';
		return 1;
	}

	orig_len=len;
	while ( len ) {

		*hex=fourbits2char[((*str) >> 4) & 0x0f];
		hex++;
		*hex=fourbits2char[(*str) & 0x0f];
		hex++;
		len--;
		str++;

	}
	return orig_len-len;
}

/* faster memchr version */
static inline char* q_memchr(char* p, int c, unsigned int size)
{
	char* end;

	end=p+size;
	for(;p<end;p++){
		if (*p==(unsigned char)c) return p;
	}
	return 0;
}

static inline unsigned short str2s(const char* s, unsigned int len,
									int *err)
{
	unsigned short ret;
	int i;
	unsigned char *limit;
	unsigned char *init;
	unsigned char* str;

	/*init*/
	str=(unsigned char*)s;
	ret=i=0;
	limit=str+len;
	init=str;

	for(;str<limit ;str++){
		if ( (*str <= '9' ) && (*str >= '0') ){
				ret=ret*10+*str-'0';
				i++;
				if (i>5) goto error_digits;
		}else{
				//error unknown char
				goto error_char;
		}
	}
	if (err) *err=0;
	return ret;

error_digits:
	LM_DBG("too many letters in [%.*s]\n", (int)len, init);
	if (err) *err=1;
	return 0;
error_char:
	LM_DBG("unexpected char %c in %.*s\n", *str, (int)len, init);
	if (err) *err=1;
	return 0;
}


/*
 * Convert a str into signed integer
 */
static inline int str2sint(str* _s, int* _r)
{
	int i;
	int s;
	
	*_r = 0;
	s = 1;
	i=0;
	if(_s->s[i]=='-') {
		s=-1;
		i++;
	}
	for(; i < _s->len; i++) {
		if ((_s->s[i] >= '0') && (_s->s[i] <= '9')) {
			*_r *= 10;
			*_r += _s->s[i] - '0';
		} else {
			return -1;
		}
	}
	*_r *= s;
	return 0;
}



/* portable determination of max_path */
int pathmax(void);

/* portable sleep in microseconds (no interrupt handling now) */

inline static void sleep_us( unsigned int nusecs )
{
	struct timeval tval;
	tval.tv_sec=nusecs/1000000;
	tval.tv_usec=nusecs%1000000;
	select(0, NULL, NULL, NULL, &tval );
}

#define  translate_pointer( _new_buf , _org_buf , _p) \
	( (_p)?(_new_buf + (_p-_org_buf)):(0) )

#define str_init(_string)  {_string, sizeof(_string) - 1}

#endif

