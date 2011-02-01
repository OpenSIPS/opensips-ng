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
 *  2010-03-xx  created (bogdan)
 */

/*TODO
	2) CRLFCR ping pong
*/

#include <errno.h>
#include <stdlib.h> /*exit() */
#include <unistd.h>
#include <fcntl.h>

#include "../../log.h"
#include "../../mem/mem.h"
#include "../../reactor/reactor.h"
#include "../../version.h"
#include "../../globals.h"
#include "../../msg_handler.h"
#include "../../context_api.h"
#include "../../timer.h"
#include "../../parser/parse_content.h"
#include "../net_params.h"
#include "../proto.h"
#include "../socket.h"
#include "conns.h"

static int tcp_init(void);

static void tcp_destroy(void);

static int tcp_init_listener(struct socket_info *si);

static int tcp_accept(struct socket_info *si);

static int a_tcp_write(void *ctx, struct socket_info *source,
		char *buf, unsigned int len, union sockaddr_union* to, void *extra);


struct proto_interface interface = {
	"TCP",                       /* proto name */
	OPENSIPS_FULL_VERSION,       /* compile version */
	OPENSIPS_COMPILE_FLAGS,      /* compile flags */
	{                            /* functions */
		5060,                    /* default protocol */
		tcp_init,                /* tcp init function */
		tcp_destroy,             /* tcp destroy function */
		tcp_init_listener,       /* init listener function */
		tcp_accept,              /* tcp default event handler */
		a_tcp_write              /* tcp write function */
	}
};



static int tcp_init(void)
{
	return init_tcp_conns();
}


static void tcp_destroy(void)
{
	destroy_tcp_conns();
}


static int tcp_init_listener(struct socket_info *si)
{
	union sockaddr_union* addr;
	int optval;
#ifdef ENABLE_NAGLE
	int flag;
	struct protoent* pe;

	if (tcp_proto_no==-1){ /* if not already set */
		pe=getprotobyname("tcp");
		if (pe==0){
			LM_ERR("could not get TCP protocol number\n");
			tcp_proto_no=-1;
		}else{
			tcp_proto_no=pe->p_proto;
		}
	}
#endif

	addr=&si->su;
	/* sock_info->proto=PROTO_TCP; */
	if (init_su(addr, &si->address, si->port)<0){
		LM_ERR("could no init sockaddr_union\n");
		goto error;
	}
	si->socket=socket(AF2PF(addr->s.sa_family), SOCK_STREAM, 0);
	if (si->socket==-1){
		LM_ERR("socket: %s\n", strerror(errno));
		goto error;
	}
#ifdef ENABLE_NAGLE
	flag=1;
	if ( (tcp_proto_no!=-1) &&
		 (setsockopt(si->socket, tcp_proto_no , TCP_NODELAY,
					 &flag, sizeof(flag))<0) ){
		LM_ERR("could not disable Nagle: %s\n",	strerror(errno));
	}
#endif

	/* address reusage */
	optval=1;
	if (setsockopt(si->socket, SOL_SOCKET, SO_REUSEADDR,
				(void*)&optval, sizeof(optval))==-1) {
		LM_ERR("setsockopt %s\n", strerror(errno));
		goto error;
	}

	/* tos */
	optval = net_tos;
	if (setsockopt(si->socket, IPPROTO_IP, IP_TOS, (void*)&optval,
				sizeof(optval)) ==-1){
		LM_WARN("setsockopt tos: %s\n", strerror(errno));
		/* continue since this is not critical */
	}
	if (bind(si->socket, &addr->s, sockaddru_len(*addr))==-1){
		LM_ERR("bind(%x, %p, %d) on %s:%d : %s\n",
				si->socket, &addr->s, 
				(unsigned)sockaddru_len(*addr),
				si->address_str.s,si->port,
				strerror(errno));
		goto error;
	}
	if (listen(si->socket, 10)==-1){
		LM_ERR("listen(%x, %p, %d) on %s: %s\n",
				si->socket, &addr->s, 
				(unsigned)sockaddru_len(*addr),
				si->address_str.s,
				strerror(errno));
		goto error;
	}

	return 0;
error:
	if (si->socket!=-1){
		close(si->socket);
		si->socket=-1;
	}
	return -1;
}


/*! \brief Set all socket/fd options:  disable nagle, tos lowdelay,non-blocking
 * \return -1 on error */
static int init_tcp_socket(int s)
{
	int flags;
	int optval;

#ifdef ENABLE_NAGLE
	flags=1;
	if ( (tcp_proto_no!=-1) && (setsockopt(s, tcp_proto_no , TCP_NODELAY,
					&flags, sizeof(flags))<0) ){
		LM_WARN("could not disable Nagle: %s\n", strerror(errno));
	}
#endif
	/* tos*/
	optval = net_tos;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (void*)&optval,sizeof(optval)) ==-1){
		LM_WARN("setsockopt tos: %s\n",	strerror(errno));
		/* continue since this is not critical */
	}
	/* non-blocking */
	flags=fcntl(s, F_GETFL);
	if (flags==-1){
		LM_ERR("fnctl failed: (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	if (fcntl(s, F_SETFL, flags|O_NONBLOCK)==-1){
		LM_ERR("set non-blocking failed: (%d) %s\n", errno, strerror(errno));
		goto error;
	}
	return 0;
error:
	return -1;
}


static int tcp_read( int fd, char *buf, int len, int *eof)
{
	int n;

	*eof = 0;

again:
	n = read(fd, buf, len);

	if (n==-1) {
		if (errno == EWOULDBLOCK || errno == EAGAIN) {
			return 0; /* nothing has been read */
		} else if (errno == EINTR)
			goto again;
		else {
			LM_ERR("error reading: %s\n",strerror(errno));
			return -1;
		}
	} else if (n==0){
		*eof = 1;
		LM_DBG("EOF on FD %d\n",fd);
	}
	return n;
}



#define TCP_READ_CHUNK  4098
#define TCP_READ_MINFREE 16
static int tcp_event_read( void *param )
{
	struct tcp_conn *conn = (struct tcp_conn*)param;
	int n,len,eof,available;
	struct sip_msg *msg;

	LM_DBG("read event on conn %p (%d) fd=%d\n", conn, conn->id, conn->socket);

	if (conn->state==TCP_CONN_TERM) {
		/* connection terminated by other thread -> simply unref the conn
		 * without resubmitting it to the IN reactor */
		unref_tcp_conn( conn );
		return 0;
	}

	if (conn->read.msg==NULL) {
		/* starting a new message */

		LM_DBG("New message\n");

		conn->read.msg = (struct sip_msg*)shm_malloc
			( sizeof(struct sip_msg) + TCP_READ_CHUNK );
		if (conn->read.msg==NULL) {
			LM_ERR("no more shm memory\n");
			goto terminate_conn;
		}
		memset(conn->read.msg , 0, sizeof(struct sip_msg));
		conn->read.msg->buf = (char*)(conn->read.msg+1);
		conn->read.size = TCP_READ_CHUNK;
		available = TCP_READ_CHUNK;
	} else {

		LM_DBG("Existing message\n");

		/* already existing message */
		available = conn->read.size - conn->read.msg->len;
		if (available<TCP_READ_MINFREE) {
			/* increase the buffer */
			if (conn->read.size + TCP_READ_CHUNK > tcp_max_size) {
				LM_WARN("TCP message from %s larger than %d\n -> discarding\n",
					ip_addr2a(&conn->rcv.src_ip), tcp_max_size);
				goto terminate_conn;
			}
			conn->read.msg = (struct sip_msg*)shm_realloc( conn->read.msg,
				sizeof(struct sip_msg) + conn->read.size + TCP_READ_CHUNK);
			if (conn->read.msg==NULL) {
				LM_ERR("no more shm memory\n");
				goto terminate_conn;
			}
			conn->read.size += TCP_READ_CHUNK;
			available += TCP_READ_CHUNK;
		}
	}

	len = tcp_read( conn->socket, conn->read.msg->buf+conn->read.msg->len,
		available, &eof);


	/* process whatever was read */
	msg = NULL;
	if (len<0) {
		/* error on reading */
		goto terminate_conn;
	} else if (len==0) {
		if (eof)
			goto terminate_conn;
	} else {

		/* more data was read -> resume parsing */
		conn->read.msg->len += len;
again_message:
		if ( conn->read.msg->eoh == NULL) {
			/* not all headers parsed yet */
			LM_DBG("not all hdrs parsed-> forcing EOH\n");
			n = parse_msg( conn->read.msg, HDR_EOH_F );

			LM_DBG("parsing returned:%d\n",n);

			if (n<0)
				goto terminate_conn;
			if (n==0 && conn->read.msg->eoh ) {
				LM_DBG("all hdrs parsed\n");
				/* all headers found -> check the Content-Len*/
				if (conn->read.msg->content_length==NULL)
				{
					LM_DBG("header len = %d content_length = %p\n",
						 conn->read.msg->eoh - conn->read.msg->buf,
						 conn->read.msg->content_length );
					goto terminate_conn;
				}
				conn->read.msg_len = get_content_length(conn->read.msg) +
					get_body(conn->read.msg) - conn->read.msg->buf; 
			}
		}
		/* entire message read ? */
		if (conn->read.msg->eoh && conn->read.msg_len<=conn->read.msg->len ) {
			LM_DBG("msg completed\n");
			/* message completed -> update conn lifetime */
			if (conn->state!=TCP_CONN_WRITING)
				conn->timeout = get_ticks() + tcp_lifetime;
			/* detache the message */
			if (msg!=NULL) {
				heap_node_t  task;
				task.fd = 0;
				task.flags = 0;
				task.priority = TASK_PRIO_READ_IO;
				task.cb = (fd_callback*)handle_new_msg;
				task.cb_param = (void*)msg;
				put_task( reactor_in->disp, task);
				msg = NULL;
			}
			msg = conn->read.msg;
			/* fill in last network data */
			msg->rcv = conn->rcv;
			msg->rcv.proto_reserved1 = conn->id;
			msg->rcv.proto_reserved2 = 0;
			conn->port_alias = msg->via1->port;
			/* check if actually read more than a message !! */
			if ( conn->read.msg_len/*msg len*/ < msg->len /*read len*/ ) {
				/* pending data */
				n = msg->len - conn->read.msg_len;
				len = ((n/TCP_READ_CHUNK)+1)*TCP_READ_CHUNK;
				/* adjust len of current read message */
				msg->len = conn->read.msg_len;
				LM_DBG("extra data (%d)-> New message\n",n);
				/* create a new message for extra data */
				conn->read.msg = (struct sip_msg*)shm_malloc
					( sizeof(struct sip_msg) + len );
				if (conn->read.msg==NULL) {
					LM_ERR("no more shm memory\n");
					goto terminate_conn;
				}
				memset(conn->read.msg , 0, sizeof(struct sip_msg));
				conn->read.msg->buf = (char*)(conn->read.msg+1);
				conn->read.msg->len = n;
				conn->read.size = len;
				conn->read.msg_len = 0;
				memcpy( conn->read.msg->buf, msg->buf+msg->len, n);

				goto again_message;
			} else {
				/* no extra data -> detach msg from conn */
				conn->read.msg = NULL;
				conn->read.size = 0;
				conn->read.msg_len = 0;
			}
		}
		/* continue reading */
	}

	/* network op done, re-submit the listening socket if conn still valid
	 * IMPORTANT - check state ans resubmit must be atomic,
	 *             so under state lock*/
	lock_tcp_conn(conn);
	if (!eof && conn->state!=TCP_CONN_TERM) {
		LM_DBG("submit IN on conn %p (%d)\n", conn, conn->id);
		submit_task(reactor_in, (fd_callback*)tcp_event_read, (void*)conn,
			TASK_PRIO_READ_IO, conn->socket, 0);
	} else {
		LM_DBG("NO submit IN for conn %p (%d)\n", conn, conn->id);
		unref_tcp_conn(conn);
	}
	unlock_tcp_conn(conn);

	if (msg) {
		/* pass the message to upper level */
		handle_new_msg(msg);
	}

	return 0;

terminate_conn:
	/* set TERMINATE state (conn no longer in IN reactor) */
	if (set_conn_state( conn, TCP_CONN_TERM)!=-1) {
		/* force timeout on any waiting write */
		conn->timeout = get_ticks();
		/* remove connection from conn tables */
		remove_tcp_conn(conn,1);
	}
	return 0;
}


static int tcp_accept(struct socket_info *si)
{
	union sockaddr_union su;
	socklen_t su_len;
	struct tcp_conn *conn;
	int s;

	/* accept new connection */
	su_len=sizeof(su);
	s = accept(si->socket, &(su.s), &su_len);
	if (s==-1) {
		if ((errno==EAGAIN)||(errno==EWOULDBLOCK))
			return 0;
		LM_ERR("failed to accept connection(%d): %s\n",
			errno, strerror(errno));
		return -1;
	}

	/* network op done, re-submit the listening socket */
	LM_DBG("submit in on socket %d\n", si->socket);
	submit_task(reactor_in, (fd_callback *) tcp_accept, (void*)si,
		TASK_PRIO_READ_IO, si->socket, 0);

	/* handle new socket */
	if (init_tcp_socket(s)<0){
		LM_ERR("failed to init the accepted tcp socket\n");
		close(s);
		return -1;
	}

	/* add socket to TCP list */
	if ( (conn=create_tcp_conn( s, &su, si, TCP_CONN_READY))==NULL ) {
		LM_ERR("add_new_conn failed, closing socket\n");
		close(s);
		return -1;
	}

	/* add socket to the reactor */
	LM_DBG("submit in on conn %p (%d)\n", conn, conn->id);
	submit_task(reactor_in, tcp_event_read, (void*)conn,
		TASK_PRIO_READ_IO, s, 0);

	return 0;
}


static int a_tcp_send_resume(struct tcp_conn *conn);


static inline int a_tcp_send(struct tcp_conn *conn, char *buf, unsigned len,
												unsigned int offset, void *ctx)
{
	heap_node_t  task;
	int n;
	int x;

	if (conn->state==TCP_CONN_TERM) {
		/* connection terminated by other thread -> simply unref the conn */
		unref_tcp_conn( conn );
		return -1;
	}

	do {
		x = 0 ;//rand() % 4 ;
		LM_DBG("------%d\n",x);
		if (x==0) {
		n = send(conn->socket, buf+offset, len-offset,
			#ifdef MSG_NOSIGNAL
			MSG_NOSIGNAL
			#else
			0
			#endif
		);
		} else { n=-1;errno=EAGAIN;}

		if (n<0) {
			/* write failed - why ?*/
			if (errno==EINTR)
				/* just signal intrerupted */
				continue;
			if (errno==EAGAIN && errno==EWOULDBLOCK) {
				/* blocking :( => try to suspend */
				LM_INFO("suspending as write blocks on conn %p (%d)\n",
						conn, conn->id);
				lock_tcp_conn(conn);
				if (conn->state!=TCP_CONN_TERM) {
					/* connection still valid -=> push it in OUT reactor 
					 * and wait for write indication */
					conn->write.active.ctx = ctx;
					conn->write.active.buf = buf;
					conn->write.active.len = len;
					conn->write.offset = offset;
					conn->timeout = get_ticks() + tcp_write_timeout;
					LM_DBG("submit out on conn %p (%d)\n", conn, conn->id);
					submit_task(reactor_out,
						(fd_callback*)a_tcp_send_resume,
						(void*)conn, TASK_PRIO_RESUME_IO, conn->socket, 0);
					unlock_tcp_conn(conn);
					return 1;
				} else {
					/* connection terminated in other part -> simply let 
					 * it go by unref and without submitting */
					unlock_tcp_conn(conn);
					unref_tcp_conn(conn);
					return -1;
				}
			} else {
				LM_ERR("failed to send: (%d) %s\n", errno, strerror(errno));
				/* write failure -> trash the connection */
				if( set_conn_state( conn, TCP_CONN_TERM)!=-1) {
					remove_tcp_conn(conn,1);
					/* fire the IN reactor */
					fire_fd( reactor_in, conn->socket);
				}
				return -1;
			}
		}

		/* some data was written */
		offset += n;
		if (offset==len) {
			/* complete buffer was written - success */
			conn->timeout = get_ticks() + tcp_lifetime;
			LM_DBG("write completed on conn %p (%d)\n", conn, conn->id);
			/* change the state WRITING -> READY (if allowed) */
			if (set_conn_state( conn, TCP_CONN_READY)==1 ) {
				/* we still have pending writes */
				conn_upload_next_write(conn);
				unlock_tcp_conn( conn );
				/* create a new task to resume writes on this fd */
				task.fd = conn->socket;
				task.flags = 0;
				task.priority = TASK_PRIO_RESUME_EXEC;
				task.cb = (fd_callback*)a_tcp_send_resume;
				task.cb_param = (void*)conn;
				put_task( reactor_out->disp, task);
				/* ref is passed to the following writes */
			} else {
				/* no other writes to be done, done, unref conn */
				unref_tcp_conn( conn );
			}
			return 0;
		}

		/* still have something to write -> try again */
	} while (1);

	return 0;
}


static int a_tcp_send_resume(struct tcp_conn *conn)
{
	void *ctx;
	int n;

	LM_DBG("resume write on conn %p (%d)\n", conn, conn->id);
	ctx = conn->write.active.ctx;

	/* if we are here, it means the socket is ready for writting */
	if ((n=a_tcp_send( conn, conn->write.active.buf, conn->write.active.len,
	conn->write.offset, ctx )) == 1 ) {
		/* write still blocking - just wait */
		return 1;
	}

	/* current write completed (with success or failure) ->
	 * resume the context execution */
	context_resume( ctx ,n );

	return 0;
}


static int a_tcp_connect_done(struct tcp_conn *conn)
{
	int err;
	unsigned int err_len;
	void *ctx;

	conn->timeout = get_ticks() + tcp_connect_timeout;  //FIXME

	if (conn->state==TCP_CONN_TERM) {
		/* connection terminated by other thread -> simply unref the conn */
		unref_tcp_conn( conn );
		return -1;
	}

	/* check the connect result */
	err_len=sizeof(err);
	getsockopt( conn->socket, SOL_SOCKET, SO_ERROR, &err, &err_len);
	if ( err!=0 ) {
		if (err==EINPROGRESS || err==EALREADY) {
			/* try again */
			LM_DBG("submit out on conn %p (%d)\n", conn, conn->id);
			conn->timeout = get_ticks() + tcp_connect_timeout;
			submit_task(reactor_out, (fd_callback*)a_tcp_connect_done,
				(void*)conn, TASK_PRIO_RESUME_IO, conn->socket,0);
			return 1;
		}
		LM_ERR("failed to get connect error (%d) %s\n", err, strerror(err));
		ctx = conn->write.active.ctx;

		/* trash connection and resume all pending contexts */
		set_conn_state( conn, TCP_CONN_TERM);
		remove_tcp_conn(conn,0);

		/* proceed with execution of the context */
		context_resume( ctx , -1/*error*/ );

		return -1;
	}

	/* connect succesfully done -> 
	 * set the fd for read also and make an extra ref for write */
	ref_tcp_conn(conn);

	LM_DBG("submit in on conn %p (%d)\n", conn, conn->id);
	submit_task(reactor_in, (fd_callback*)tcp_event_read, (void*)conn,
		TASK_PRIO_READ_IO, conn->socket, 0);

	/* go for write */
	return a_tcp_send_resume( conn );
}


static int a_tcp_connect(void *ctx, struct socket_info* source,
			char *buf, unsigned len, union sockaddr_union *to)
{
	int sock;
	socklen_t local_su_len;
	union sockaddr_union local_su;
	struct tcp_conn* conn;
	int n;

	/* create new stream socket */
	sock = socket( AF2PF(to->s.sa_family), SOCK_STREAM, 0);
	if (sock==-1) {
		LM_ERR("socket failed with (%d) %s\n", errno, strerror(errno));
		return -1;
	}

	/* init the socket*/
	if (init_tcp_socket(sock)<0){
		LM_ERR("failed to init tcp socket for connect\n");
		goto error;
	}

	/* set network source */
	local_su_len = sockaddru_len(source->su);
	memcpy( &local_su, &source->su, local_su_len);
	su_setport( &local_su, 0);

	/* bind the socket */
	if (bind( sock, &local_su.s, local_su_len )!=0) {
		LM_ERR("bind failed with (%d) %s\n", errno, strerror(errno));
		goto error;
	}

	/* add socket to TCP list */
	if ( (conn=create_tcp_conn(sock,to,source,TCP_CONN_WRITING))==NULL ){
		LM_ERR("add_new_conn failed, abording connect\n");
		goto error;
	}

	conn->write.active.ctx = ctx;
	conn->write.active.buf = buf;
	conn->write.active.len = len;
	conn->write.offset = 0;

	/* start the connect procedure */
	do {
		n = connect( sock, &to->s, sockaddru_len(*to) );
		if (n==-1) {
			/* connect failed */
			if (errno==EINTR) {
				/* just signal wakeup */
				continue;
			}
			if (errno==EINPROGRESS || errno==EALREADY) {
				/* connect will block -> suspend */
				conn->timeout = get_ticks() + tcp_connect_timeout;
				LM_DBG("submit out on conn %p (%d)\n", conn, conn->id);
				submit_task(reactor_out, (fd_callback*)a_tcp_connect_done,
					(void*)conn, TASK_PRIO_RESUME_IO, conn->socket, 0);
				return 1;
			}
			LM_ERR("connect failed with (%d) %s\n", errno, strerror(errno));
			goto error2;
		}
	}while(n!=0);

	/* connect succesfully completed without context switching -> 
	 * proceed with writing */
	return a_tcp_send_resume( conn );

error2:
	/* trash the connection */
	remove_tcp_conn(conn,0);
	return -1;
error:
	close(sock);
	return -1;
}



static int a_tcp_write(void *ctx, struct socket_info *source,
				char *buf, unsigned len, union sockaddr_union* to, void *extra)
{
	struct tcp_conn *conn;
	int n;

	/* any existing connection to the destination? */
	conn = search_tcp_conn( (unsigned int)(long)extra, to );
	if (conn==NULL) {
		/* open a new TCP connection to destination - THIS IS ASYNC CALL */
		return a_tcp_connect( ctx, source, buf, len, to);
	}

	/* proceed with writing */

	/* is connection available for writing ? like is connect done and no
	   other writing in progress ? */
	if (set_conn_state( conn, TCP_CONN_WRITING)==1 ) {
		/* we cannot write right now :( -> queue on connection */
		n = conn_add_pending_write( conn, buf, len, ctx);
		unlock_tcp_conn( conn );
		/* unref connection (from search function) */
		unref_tcp_conn( conn );
		/* return ASYNC or FAILURE notification */
		return (n==0)?1:-1;
	}

	/* we have the connection and write permission also :) */
	return a_tcp_send( conn, buf, len, 0, ctx);
}




