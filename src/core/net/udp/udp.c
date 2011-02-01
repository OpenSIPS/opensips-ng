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
 *  2010-02-xx  created (bogdan)
 */


#include "errno.h"
#include <unistd.h>

#include "../../log.h"
#include "../../globals.h"
#include "../../mem/mem.h"
#include "../../version.h"
#include "../../msg_handler.h"
#include "../../reactor/reactor.h"
#include "../net_params.h"
#include "../proto.h"
#include "../socket.h"


static int udp_init_listener(struct socket_info *si);

static int udp_read(struct socket_info *si);

static int udp_write(void *ctx, struct socket_info *source,
		char *buf, unsigned len, union sockaddr_union*  to, void *extra);


struct proto_interface interface = {
	"UDP",                       /* proto name */
	OPENSIPS_FULL_VERSION,       /* compile version */
	OPENSIPS_COMPILE_FLAGS,      /* compile flags */
	{                            /* functions */
		5060,                    /* default protocol */
		NULL,                    /* init function */
		NULL,                    /* destroy function */
		udp_init_listener,       /* init listener function */
		udp_read,                /* default event handler */
		udp_write                /* udp write function */
	}
};


/**
 * Tries to find the maximum receiver buffer size. This value is
 * system dependend, thus it need to detected on startup.
 *
 * \param udp_sock checked socket
 * \return zero on success, -1 otherwise
 */
static int probe_max_receive_buffer(int udp_sock)
{
#define BUFFER_INCREMENT  2048
	unsigned int size, check_size;
	unsigned int len;
	int stage;

	/* get original size */
	len = sizeof(size);
	if (getsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF, (void*)&size, &len)==-1){
		LM_ERR("getsockopt failed: %s\n", strerror(errno));
		return -1;
	}

	if ( size==0 )
		size=BUFFER_INCREMENT;
	LM_DBG("initial UDP buffer size is %d\n", size );

	/* try to increase buffer size. ALG:
	 * stage 1 : double the size
	 * stage 2 : incremental */
	stage = 0;
	while (1) {
		/* increase size according to current alg */
		if (stage==0) 
			size <<= 1; 
		else
			size+=BUFFER_INCREMENT;

		if (size > udp_maxbuffer){
			if (stage==1)
				break; 
			else {
				stage=1;
				size >>=1;
				continue;
			}
		}

		if (setsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF, (void*)&size, 
		sizeof(size)) == 0) {
			/* verify if change has taken effect */
			len=sizeof(check_size);
			if (getsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF,
			(void*) &check_size, &len)==-1 ) {
				LM_ERR("getsockopt failed: %s\n", strerror(errno));
				return -1;
			}

			if (check_size>=size) {
				/* change was with success, continue */
				continue;
			}
		}

		/* setsockopt failed with error or had not effect */
		/* if still in stage 0, try less aggressively in stage 1 */
		if (stage==0) {
			stage=1;
			size >>=1;
			continue;
		} else {
			break;
		} 
	}

	len=sizeof(size);
	if (getsockopt( udp_sock, SOL_SOCKET, SO_RCVBUF, (void*) &size, &len)==-1){
		LM_ERR("getsockopt failed: %s\n", strerror(errno));
		return -1;
	}
	LM_INFO("using a UDP receive buffer of %d kb\n", (size/1024));

	return 0;
}


#ifdef USE_MCAST

/**
 * Setup a multicast receiver socket, supports IPv4 and IPv6.
 * \param sock socket
 * \param addr receiver address
 * \return zero on success, -1 otherwise
 */
static int setup_mcast_rcvr(int sock, union sockaddr_union* addr)
{
	struct ip_mreq mreq;
	struct ipv6_mreq mreq6;
	
	if (addr->s.sa_family==AF_INET){
		memcpy(&mreq.imr_multiaddr, &addr->sin.sin_addr,
		       sizeof(struct in_addr));
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
		
		if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq,
			       sizeof(mreq))==-1){
			LM_ERR("setsockopt: %s\n", strerror(errno));
			return -1;
		}
		
	} else if (addr->s.sa_family==AF_INET6) {
		memcpy(&mreq6.ipv6mr_multiaddr, &addr->sin6.sin6_addr,
		       sizeof(struct in6_addr));
		mreq6.ipv6mr_interface = 0;
		if (
#ifdef __OS_linux
		setsockopt(sock,IPPROTO_IPV6,IPV6_ADD_MEMBERSHIP,&mreq6,sizeof(mreq6))
#else
		setsockopt(sock,IPPROTO_IPV6,IPV6_JOIN_GROUP,&mreq6,sizeof(mreq6))
#endif
		==-1) {
			LM_ERR("setsockopt:%s\n",  strerror(errno));
			return -1;
		}
		
	} else {
		LM_ERR("unsupported protocol family\n");
		return -1;
	}
	return 0;
}

#endif /* USE_MCAST */


/**
 * Initialize a UDP socket, supports multicast, IPv4 and IPv6.
 * \param sock_info socket that should be bind
 * \return zero on success, -1 otherwise
 */
static int udp_init_listener(struct socket_info *si)
{
	union sockaddr_union* addr;
	int optval;
#ifdef USE_MCAST
	unsigned char m_optval;
#endif

	addr = &si->su;
	if (init_su(addr, &si->address, si->port)<0){
		LM_ERR("could not init sockaddr_union\n");
		goto error;
	}

	si->socket = socket(AF2PF(addr->s.sa_family), SOCK_DGRAM, 0);
	if (si->socket==-1){
		LM_ERR("socket: %s\n", strerror(errno));
		goto error;
	}
	/* set sock opts? */
	optval=1;
	if (setsockopt(si->socket, SOL_SOCKET, SO_REUSEADDR ,
					(void*)&optval, sizeof(optval)) ==-1){
		LM_ERR("setsockopt: %s\n", strerror(errno));
		goto error;
	}
	/* tos */
	optval = net_tos;
	if (setsockopt(si->socket, IPPROTO_IP, IP_TOS, (void*)&optval, 
			sizeof(optval)) ==-1){
		LM_WARN("setsockopt tos: %s\n", strerror(errno));
		/* continue since this is not critical */
	}
#if defined (__linux__) && defined(UDP_ERRORS)
	optval=1;
	/* enable error receiving on unconnected sockets */
	if(setsockopt(sock_info->socket, SOL_IP, IP_RECVERR,
					(void*)&optval, sizeof(optval)) ==-1){
		LM_ERR("setsockopt: %s\n", strerror(errno));
		goto error;
	}
#endif

#ifdef USE_MCAST
	if ((si->flags&SI_FLAG_IS_MCAST) && (setup_mcast_rcvr(si->socket,addr)<0)){
			goto error;
	}
	/* set the multicast options */
	if (addr->s.sa_family==AF_INET){
		m_optval = mcast_loopback;
		if (setsockopt(si->socket, IPPROTO_IP, IP_MULTICAST_LOOP, 
						&m_optval, sizeof(m_optval))==-1){
			LM_WARN("setsockopt(IP_MULTICAST_LOOP): %s\n", strerror(errno));
			/* it's only a warning because we might get this error if the
			  network interface doesn't support multicasting */
		}
		if (mcast_ttl>=0){
			m_optval = mcast_ttl;
			if (setsockopt(si->socket, IPPROTO_IP, IP_MULTICAST_TTL,
						&m_optval, sizeof(m_optval))==-1){
				LM_ERR("setsockopt (IP_MULTICAST_TTL): %s\n", strerror(errno));
				goto error;
			}
		}
	} else if (addr->s.sa_family==AF_INET6){
		if (setsockopt(si->socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, 
						&mcast_loopback, sizeof(mcast_loopback))==-1){
			LM_WARN("setsockopt (IPV6_MULTICAST_LOOP): %s\n", strerror(errno));
			/* it's only a warning because we might get this error if the
			  network interface doesn't support multicasting */
		}
		if (mcast_ttl>=0){
			if (setsockopt(si->socket, IPPROTO_IP, IPV6_MULTICAST_HOPS,
						&mcast_ttl, sizeof(mcast_ttl))==-1){
				LM_ERR("setssckopt (IPV6_MULTICAST_HOPS): %s\n",
						strerror(errno));
				goto error;
			}
		}
	} else {
		LM_ERR("unsupported protocol family %d\n", addr->s.sa_family);
		goto error;
	}
#endif /* USE_MCAST */

	if (probe_max_receive_buffer(si->socket)==-1) goto error;

	if (bind(si->socket,  &addr->s, sockaddru_len(*addr))==-1){
		LM_ERR("bind(%x, %p, %d) on %s: %s\n", si->socket, &addr->s, 
				(unsigned)sockaddru_len(*addr), si->address_str.s,
				strerror(errno));
		if (addr->s.sa_family==AF_INET6)
			LM_ERR("might be caused by using a link "
					" local address, try site local or global\n");
		goto error;
	}
	return 0;

error:
	return -1;
}


/**
 * Read message from the network
 * \returns read len or -1 for errors
 */
static int udp_read(struct socket_info *si)
{
	unsigned int fromlen;
	struct sip_msg *m;
	struct receive_info *ri;
	union sockaddr_union su;
	int n;
	char tmp;

	/* get the len of the datagram */
	do {

		fromlen = sockaddru_len( si->su );
		n = recvfrom( si->socket, &tmp, 1, MSG_PEEK | MSG_TRUNC,
			&su.s, &fromlen);

		if (n==-1){
			if (errno==EAGAIN){
				LM_DBG("packet with bad checksum received\n");
				continue;
			}
			LM_ERR("recvfrom says [%d] %s\n", errno, strerror(errno));
			if ((errno==EINTR)||(errno==EWOULDBLOCK)|| (errno==ECONNREFUSED))
				continue;
			else
				goto error1;
		}

	} while(n<0);

	/* allocate buffer for sip_msg + buffer */
	m = (struct sip_msg*)shm_malloc( sizeof(struct sip_msg) + n );
	if (m==NULL){
		LM_ERR("could not allocate receive buffer\n");
		// FIXME - flush the read event
		read(si->socket, &n, 4);
		goto error1;
	}
	memset(m, 0, sizeof(struct sip_msg) + n);

	ri = &m->rcv;

	m->buf = (char*)(m+1);
	m->len = n;

	/* do the actual data read */
	do {

		fromlen = sockaddru_len( si->su );
		n = recvfrom( si->socket, m->buf, m->len, 0,
				 &ri->src_su.s, &fromlen);

		if (n==-1) {
			if (errno==EAGAIN){
				LM_DBG("packet with bad checksum received\n");
				continue;
			}
			LM_ERR("recvfrom says [%d] %s\n", errno, strerror(errno));
			if ((errno==EINTR)||(errno==EWOULDBLOCK)|| (errno==ECONNREFUSED))
				continue;
			else
				goto error;
		}

	} while(n<0);

	/* done with reading from network -> resubmit the fd for other reads */
	submit_task(reactor_in, (fd_callback*)udp_read,
		(void*)si, TASK_PRIO_READ_IO, si->socket, 0);

	if (m->len<udp_min_size) {
		LM_DBG("probing packet received len = %d\n", m->len);
		goto error;
	}

	su2ip_addr( &ri->src_ip, &ri->src_su);
	ri->src_port = su_getport( &ri->src_su );

	if (ri->src_port==0){
		LM_INFO("dropping 0 port packet from %s\n", ip_addr2a( &ri->src_ip ));
		goto error;
	}

	ri->bind_address = si;
	ri->dst_port = si->port;
	ri->dst_ip = si->address;
	ri->proto = PROTO_UDP;
	ri->proto_reserved1 = ri->proto_reserved2=0;

	/* parse the message */
	n = parse_msg(m,0);
	if (n>0) {
		LM_ERR("incomplet SIP message read via UDP -> discard\n");
		goto error2;
	}
	if (n<0) {
		LM_ERR("bad SIP message read -> discarding\n");
		goto error2;
	}

	/* this will not change the pointer as we are shriking the memory 
	 * block to exactly used size */

	/* pass the message to upper level */
	handle_new_msg(m);

	return 0;

error:
	shm_free(m);
error1:
	return -1;
error2:
	free_sip_msg(m);
	return -1;
}




/**
 * Main UDP send function, called from msg_send.
 * \see msg_send 
 * \param source send socket
 * \param buf sent data
 * \param len data length in bytes
 * \param to destination address
 * \return -1 on error, the return value from sento on success
 */
static int udp_write(void *ctx, struct socket_info *source,
				char *buf, unsigned len, union sockaddr_union* to, void *extra)
{
	int n, tolen;

	tolen=sockaddru_len(*to);
again:
	n=sendto(source->socket, buf, len, 0, &to->s, tolen);
	if (n==-1){
		LM_ERR("sendto(sock,%p,%d,0,%p,%d): %s(%d)\n", buf,len,to,tolen,
				strerror(errno),errno);
		if (errno==EINTR) goto again;
		if (errno==EINVAL) {
			LM_CRIT("invalid sendtoparameters\n"
			"one possible reason is the server is bound to localhost and\n"
			"attempts to send to the net\n");
		}
	}
	return n;
}
