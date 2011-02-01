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


#ifndef _H_CORE_NET_IP_ADDR
#define _H_CORE_NET_IP_ADDR

#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "../mem/mem.h"
#include "../str.h"
#include "../log.h"

struct ip_addr{
	unsigned int af; /*!< address family: AF_INET6 or AF_INET */
	unsigned int len;    /*!< address len, 16 or 4 */
	
	/*! \brief 64 bits aligned address */
	union {
		unsigned long  addrl[16/sizeof(long)]; /*!< long format*/
		unsigned int   addr32[4];
		unsigned short addr16[8];
		unsigned char  addr[16];
	}u;
};



struct net{
	struct ip_addr ip;
	struct ip_addr mask;
};

union sockaddr_union{
		struct sockaddr     s;
		struct sockaddr_in  sin;
		struct sockaddr_in6 sin6;
};


/* len of the sockaddr */
#ifdef HAVE_SOCKADDR_SA_LEN
	#define sockaddru_len(su)	((su).s.sa_len)
#else
	#define sockaddru_len(su)	\
			(((su).s.sa_family==AF_INET6)?sizeof(struct sockaddr_in6):\
					sizeof(struct sockaddr_in))
#endif /* HAVE_SOCKADDR_SA_LEN*/


/*! \brief inits an ip_addr with the addr. info from a hostent structure
 * ip = struct ip_addr*
 * he= struct hostent*
 */
#define hostent2ip_addr(ip, he, addr_no) \
	do{ \
		(ip)->af=(he)->h_addrtype; \
		(ip)->len=(he)->h_length;  \
		memcpy((ip)->u.addr, (he)->h_addr_list[(addr_no)], (ip)->len); \
	}while(0)


/* gets the protocol family corresponding to a specific address family
 * ( PF_INET - AF_INET, PF_INET6 - AF_INET6, af for others)
 */
#define AF2PF(af)   (((af)==AF_INET)?PF_INET:((af)==AF_INET6)?PF_INET6:(af))


/*! \brief compare 2 ip_addrs (both args are pointers)*/
#define ip_addr_cmp(ip1, ip2) \
	(((ip1)->af==(ip2)->af)&& \
	 	(memcmp((ip1)->u.addr, (ip2)->u.addr, (ip1)->len)==0))


/*! \brief inits a struct sockaddr_union from a struct ip_addr and a port no 
 * \return 0 if ok, -1 on error (unknown address family)
 * \note the port number is in host byte order */
static inline int init_su( union sockaddr_union* su,
							struct ip_addr* ip,
							unsigned short   port ) 
{
	memset(su, 0, sizeof(union sockaddr_union));/*needed on freebsd*/
	su->s.sa_family=ip->af;
	switch(ip->af){
		case AF_INET6:
			memcpy(&su->sin6.sin6_addr, ip->u.addr, ip->len); 
			#ifdef HAVE_SOCKADDR_SA_LEN
				su->sin6.sin6_len=sizeof(struct sockaddr_in6);
			#endif
			su->sin6.sin6_port=htons(port);
			break;
		case AF_INET:
			memcpy(&su->sin.sin_addr, ip->u.addr, ip->len);
			#ifdef HAVE_SOCKADDR_SA_LEN
				su->sin.sin_len=sizeof(struct sockaddr_in);
			#endif
			su->sin.sin_port=htons(port);
			break;
		default:
			LM_CRIT("unknown address family %d\n", ip->af);
			return -1;
	}
	return 0;
}


/*! \brief converts a str to an ipv4 address, returns the address or 0 on error
   Warning: the result is a pointer to a statically allocated structure */
static inline int str2ip(str* st, struct ip_addr* dest)
{
	int i;
	unsigned char *limit;
	struct ip_addr ip;
	unsigned char* s;

	s=(unsigned char*)st->s;

	/*init*/
	ip.u.addr32[0]=0;
	i=0;
	limit=(unsigned char*)(st->s + st->len);

	for(;s<limit ;s++){
		if (*s=='.'){
				i++;
				if (i>3) goto error_dots;
		}else if ( (*s <= '9' ) && (*s >= '0') ){
				ip.u.addr[i]=ip.u.addr[i]*10+*s-'0';
		}else{
				/* error unknown char */
				goto error_char;
		}
	}
	if (i<3) goto error_dots;
	ip.af=AF_INET;
	ip.len=4;

	*dest = ip;
	return 0;

error_dots:
	LM_DBG("too %s dots in [%.*s]\n", (i>3)?"many":"few", 
			st->len, st->s);
	return -1;
error_char:
	return -1;
}


/*! \brief returns an ip_addr struct.; on error returns 0
 * the ip_addr struct is static, so subsequent calls will destroy its content*/
static inline int str2ip6(str* st,  struct ip_addr* dest)
{
#define HEX2I(c) \
	(	(((c)>='0') && ((c)<='9'))? (c)-'0' :  \
		(((c)>='A') && ((c)<='F'))? ((c)-'A')+10 : \
		(((c)>='a') && ((c)<='f'))? ((c)-'a')+10 : -1 )
	int i, idx1, rest;
	int no_colons;
	int double_colon;
	int hex;
	struct ip_addr ip;
	unsigned short* addr_start;
	unsigned short addr_end[8];
	unsigned short* addr;
	unsigned char* limit;
	unsigned char* s;
	
	/* init */
	if ((st->len) && (st->s[0]=='[')){
		/* skip over [ ] */
		if (st->s[st->len-1]!=']') goto error_char;
		s=(unsigned char*)(st->s+1);
		limit=(unsigned char*)(st->s+st->len-1);
	}else{
		s=(unsigned char*)st->s;
		limit=(unsigned char*)(st->s+st->len);
	}
	i=idx1=rest=0;
	double_colon=0;
	no_colons=0;
	ip.af=AF_INET6;
	ip.len=16;
	addr_start=ip.u.addr16;
	addr=addr_start;
	memset(addr_start, 0 , 8*sizeof(unsigned short));
	memset(addr_end, 0 , 8*sizeof(unsigned short));
	for (; s<limit; s++){
		if (*s==':'){
			no_colons++;
			if (no_colons>7) goto error_too_many_colons;
			if (double_colon){
				idx1=i;
				i=0;
				if (addr==addr_end) goto error_colons;
				addr=addr_end;
			}else{
				double_colon=1;
				addr[i]=htons(addr[i]);
				i++;
			}
		}else if ((hex=HEX2I(*s))>=0){
				addr[i]=addr[i]*16+hex;
				double_colon=0;
		}else{
			/* error, unknown char */
			goto error_char;
		}
	}
	if (!double_colon){ /* not ending in ':' */
		addr[i]=htons(addr[i]);
		i++; 
	}
	/* if address contained '::' fix it */
	if (addr==addr_end){
		rest=8-i-idx1;
		memcpy(addr_start+idx1+rest, addr_end, i*sizeof(unsigned short));
	}else{
		/* no double colons inside */
		if (no_colons<7) goto error_too_few_colons;
	}
/*
	DBG("str2ip6: idx1=%d, rest=%d, no_colons=%d, hex=%x\n",
			idx1, rest, no_colons, hex);
	DBG("str2ip6: address %x:%x:%x:%x:%x:%x:%x:%x\n", 
			addr_start[0], addr_start[1], addr_start[2],
			addr_start[3], addr_start[4], addr_start[5],
			addr_start[6], addr_start[7] );
*/
	*dest = ip;
	return 0;

error_too_many_colons:
	LM_DBG("too many colons in [%.*s]\n", st->len, st->s);
	return -1;

error_too_few_colons:
	LM_DBG("too few colons in [%.*s]\n", st->len, st->s);
	return -1;

error_colons:
	LM_DBG("too many double colons in [%.*s]\n", st->len, st->s);
	return -1;

error_char:
	/*
	DBG("str2ip6: WARNING: unexpected char %c in  [%.*s]\n", *s, st->len,
			st->s);*/
	return -1;
}


/*! \brief maximum size of a str returned by ip_addr2a (including \\0') */
#define IP_ADDR_MAX_STR_SIZE 40 /* 1234:5678:9012:3456:7890:1234:5678:9012\0 */

/*! \brief fast ip_addr -> string converter;
 * it uses an internal buffer
 */
extern char _ip_addr_A_buff[IP_ADDR_MAX_STR_SIZE];
static inline char* ip_addr2a(struct ip_addr* ip)
{
	int offset;
	register unsigned char a,b,c;
	register unsigned char d;
	register unsigned short hex4;
	int r;
	#define HEXDIG(x) (((x)>=10)?(x)-10+'A':(x)+'0')

	offset=0;
	switch(ip->af){
		case AF_INET6:
			for(r=0;r<7;r++){
				hex4=ntohs(ip->u.addr16[r]);
				a=hex4>>12;
				b=(hex4>>8)&0xf;
				c=(hex4>>4)&0xf;
				d=hex4&0xf;
				if (a){
					_ip_addr_A_buff[offset]=HEXDIG(a);
					_ip_addr_A_buff[offset+1]=HEXDIG(b);
					_ip_addr_A_buff[offset+2]=HEXDIG(c);
					_ip_addr_A_buff[offset+3]=HEXDIG(d);
					_ip_addr_A_buff[offset+4]=':';
					offset+=5;
				}else if(b){
					_ip_addr_A_buff[offset]=HEXDIG(b);
					_ip_addr_A_buff[offset+1]=HEXDIG(c);
					_ip_addr_A_buff[offset+2]=HEXDIG(d);
					_ip_addr_A_buff[offset+3]=':';
					offset+=4;
				}else if(c){
					_ip_addr_A_buff[offset]=HEXDIG(c);
					_ip_addr_A_buff[offset+1]=HEXDIG(d);
					_ip_addr_A_buff[offset+2]=':';
					offset+=3;
				}else{
					_ip_addr_A_buff[offset]=HEXDIG(d);
					_ip_addr_A_buff[offset+1]=':';
					offset+=2;
				}
			}
			/* last int16*/
			hex4=ntohs(ip->u.addr16[r]);
			a=hex4>>12;
			b=(hex4>>8)&0xf;
			c=(hex4>>4)&0xf;
			d=hex4&0xf;
			if (a){
				_ip_addr_A_buff[offset]=HEXDIG(a);
				_ip_addr_A_buff[offset+1]=HEXDIG(b);
				_ip_addr_A_buff[offset+2]=HEXDIG(c);
				_ip_addr_A_buff[offset+3]=HEXDIG(d);
				_ip_addr_A_buff[offset+4]=0;
			}else if(b){
				_ip_addr_A_buff[offset]=HEXDIG(b);
				_ip_addr_A_buff[offset+1]=HEXDIG(c);
				_ip_addr_A_buff[offset+2]=HEXDIG(d);
				_ip_addr_A_buff[offset+3]=0;
			}else if(c){
				_ip_addr_A_buff[offset]=HEXDIG(c);
				_ip_addr_A_buff[offset+1]=HEXDIG(d);
				_ip_addr_A_buff[offset+2]=0;
			}else{
				_ip_addr_A_buff[offset]=HEXDIG(d);
				_ip_addr_A_buff[offset+1]=0;
			}
			break;
		case AF_INET:
			for(r=0;r<3;r++){
				a=ip->u.addr[r]/100;
				c=ip->u.addr[r]%10;
				b=ip->u.addr[r]%100/10;
				if (a){
					_ip_addr_A_buff[offset]=a+'0';
					_ip_addr_A_buff[offset+1]=b+'0';
					_ip_addr_A_buff[offset+2]=c+'0';
					_ip_addr_A_buff[offset+3]='.';
					offset+=4;
				}else if (b){
					_ip_addr_A_buff[offset]=b+'0';
					_ip_addr_A_buff[offset+1]=c+'0';
					_ip_addr_A_buff[offset+2]='.';
					offset+=3;
				}else{
					_ip_addr_A_buff[offset]=c+'0';
					_ip_addr_A_buff[offset+1]='.';
					offset+=2;
				}
			}
			/* last number */
			a=ip->u.addr[r]/100;
			c=ip->u.addr[r]%10;
			b=ip->u.addr[r]%100/10;
			if (a){
				_ip_addr_A_buff[offset]=a+'0';
				_ip_addr_A_buff[offset+1]=b+'0';
				_ip_addr_A_buff[offset+2]=c+'0';
				_ip_addr_A_buff[offset+3]=0;
			}else if (b){
				_ip_addr_A_buff[offset]=b+'0';
				_ip_addr_A_buff[offset+1]=c+'0';
				_ip_addr_A_buff[offset+2]=0;
			}else{
				_ip_addr_A_buff[offset]=c+'0';
				_ip_addr_A_buff[offset+1]=0;
			}
			break;
		
		default:
			LM_CRIT("unknown address family %d\n", ip->af);
			return 0;
	}
	
	return _ip_addr_A_buff;
}

//TODO fix static variables
/*! \brief converts an ip_addr structure to a hostent
 * \return pointer to internal statical structure */



/*! \brief gets the port number (host byte order) */
static inline unsigned short su_getport(union sockaddr_union* su)
{
	if(su==0)
		return 0;

	switch(su->s.sa_family){
		case AF_INET:
			return ntohs(su->sin.sin_port);
		case AF_INET6:
			return ntohs(su->sin6.sin6_port);
		default:
			LM_CRIT("unknown address family %d\n", su->s.sa_family);
			return 0;
	}
}


/*! \brief sets the port number (host byte order) */
static inline void su_setport(union sockaddr_union* su, unsigned short port)
{
	switch(su->s.sa_family){
		case AF_INET:
			su->sin.sin_port=htons(port);
			break;
		case AF_INET6:
			 su->sin6.sin6_port=htons(port);
			 break;
		default:
			LM_CRIT("unknown address family %d\n", su->s.sa_family);
	}
}


/*! \brief inits an ip_addr pointer from a sockaddr_union ip address */
static inline void su2ip_addr(struct ip_addr* ip, union sockaddr_union* su)
{
	switch(su->s.sa_family){
		case AF_INET: 
			ip->af=AF_INET;
			ip->len=4;
			memcpy(ip->u.addr, &su->sin.sin_addr, 4);
			break;
		case AF_INET6:
			ip->af=AF_INET6;
			ip->len=16;
			memcpy(ip->u.addr, &su->sin6.sin6_addr, 16);
			break;
		default:
			LM_CRIT("Unknown address family %d\n", su->s.sa_family);
	}
}


/*! \brief compare 2 sockaddr_unions */
static inline int su_cmp(union sockaddr_union* s1, union sockaddr_union* s2)
{
	if (s1->s.sa_family!=s2->s.sa_family) return 0;
	switch(s1->s.sa_family){
		case AF_INET:
			return (s1->sin.sin_port==s2->sin.sin_port)&&
					(memcmp(&s1->sin.sin_addr, &s2->sin.sin_addr, 4)==0);
#ifdef USE_IPV6
		case AF_INET6:
			return (s1->sin6.sin6_port==s2->sin6.sin6_port)&&
					(memcmp(&s1->sin6.sin6_addr, &s2->sin6.sin6_addr, 16)==0);
#endif
		default:
			LM_CRIT("unknown address family %d\n",
						s1->s.sa_family);
			return 0;
	}
}

static inline int hostent2su( union sockaddr_union* su,
								struct hostent* he,
								unsigned int idx,
								unsigned short   port )
{
	memset(su, 0, sizeof(union sockaddr_union)); /*needed on freebsd*/
	su->s.sa_family=he->h_addrtype;
	switch(he->h_addrtype){
#ifdef USE_IPV6
	case	AF_INET6:
		memcpy(&su->sin6.sin6_addr, he->h_addr_list[idx], he->h_length);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin6.sin6_len=sizeof(struct sockaddr_in6);
		#endif
		su->sin6.sin6_port=htons(port);
		break;
#endif
	case AF_INET:
		memcpy(&su->sin.sin_addr, he->h_addr_list[idx], he->h_length);
		#ifdef HAVE_SOCKADDR_SA_LEN
			su->sin.sin_len=sizeof(struct sockaddr_in);
		#endif
		su->sin.sin_port=htons(port);
		break;
	default:
		LM_CRIT("unknown address family %d\n", he->h_addrtype);
		return -1;
	}
	return 0;
}


static inline struct hostent * hostent_cpy(struct hostent* src)
{
	unsigned int len,len2, i, r;
	struct hostent * dst;

	dst = shm_malloc(sizeof (*dst) );
	if(!dst)
		goto error;
	/* start copying the host entry.. */
	/* copy h_name */
	len=strlen(src->h_name)+1;
	dst->h_name=(char*)shm_malloc(sizeof(char) * len);
	if (dst->h_name) strncpy(dst->h_name,src->h_name, len);
	else{
		goto error;
	}

	/* copy h_aliases */
	len=0;
	if (src->h_aliases)
		for (;src->h_aliases[len];len++);
	dst->h_aliases=(char**)shm_malloc(sizeof(char*)*(len+1));
	if (dst->h_aliases==0){
		shm_free(dst->h_name);
		goto error;
	}
	memset((void*)dst->h_aliases, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		len2=strlen(src->h_aliases[i])+1;
		dst->h_aliases[i]=(char*)shm_malloc(sizeof(char)*len2);
		if (dst->h_aliases==0){
			shm_free(dst->h_name);
			for(r=0; r<i; r++)	shm_free(dst->h_aliases[r]);
			shm_free(dst->h_aliases);
			goto error;
		}
		strncpy(dst->h_aliases[i], src->h_aliases[i], len2);
	}
	/* copy h_addr_list */
	len=0;
	if (src->h_addr_list)
		for (;src->h_addr_list[len];len++);
	dst->h_addr_list=(char**)shm_malloc(sizeof(char*)*(len+1));
	if (dst->h_addr_list==0){
		shm_free(dst->h_name);
		for(r=0; dst->h_aliases[r]; r++)	shm_free(dst->h_aliases[r]);
		shm_free(dst->h_aliases);
		goto error;
	}
	memset((void*)dst->h_addr_list, 0, sizeof(char*) * (len+1) );
	for (i=0;i<len;i++){
		dst->h_addr_list[i]=(char*)shm_malloc(sizeof(char)*src->h_length);
		if (dst->h_addr_list[i]==0){
			shm_free(dst->h_name);
			for(r=0; dst->h_aliases[r]; r++)	shm_free(dst->h_aliases[r]);
			shm_free(dst->h_aliases);
			for (r=0; r<i;r++) shm_free(dst->h_addr_list[r]);
			shm_free(dst->h_addr_list);
			goto error;
		}
		memcpy(dst->h_addr_list[i], src->h_addr_list[i], src->h_length);
	}

	/* copy h_addr_type & length */
	dst->h_addrtype=src->h_addrtype;
	dst->h_length=src->h_length;
	/*finished hostent copy */

	return dst;

error:
	LM_CRIT("shm memory allocation failure\n");
	return NULL;
}



static inline void free_hostent(struct hostent *dst)
{
	int r;
	if (dst->h_name) shm_free(dst->h_name);
	if (dst->h_aliases){
		for(r=0; dst->h_aliases[r]; r++) {
			shm_free(dst->h_aliases[r]);
		}
		shm_free(dst->h_aliases);
	}
	if (dst->h_addr_list){
		for (r=0; dst->h_addr_list[r];r++) {
			shm_free(dst->h_addr_list[r]);
		}
		shm_free(dst->h_addr_list);
	}
}

static inline struct hostent* ip_addr2he(str* name, struct ip_addr* ip)
{
	struct hostent he;
	char hostname[256];
	char* p_aliases[1];
	char* p_addr[2];
	char address[16];
	int len;

	p_aliases[0]=0; /* no aliases*/
	p_addr[1]=0; /* only one address*/
	p_addr[0]=address;
	len = (name->len < 256) ? name->len : 256;
	strncpy(hostname, name->s, len);
	hostname[len] = 0;
	if (ip->len>16) return 0;
	memcpy(address, ip->u.addr, ip->len);

	he.h_addrtype=ip->af;
	he.h_length=ip->len;
	he.h_addr_list=p_addr;
	he.h_aliases=p_aliases;
	he.h_name=hostname;
	return hostent_cpy(&he);
}



#ifdef USE_MCAST
/* Returns 1 if the given address is a multicast address */
int is_mcast(struct ip_addr* ip);
#endif /* USE_MCAST */

#endif

