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

#include <stdlib.h>
#include <unistd.h>

#include "../../mem/mem.h"
#include "../../locking/locking.h"
#include "../../globals.h"
#include "../../log.h"
#include "../../timer.h"
#include "../../context_api.h"
#include "../../net/ip_addr.h"
#include "../../net/net_params.h"
#include "../../dispatcher/dispatcher.h"
#include "conns.h"

#define TCP_HASH_SIZE  1024

static int tcp_connections_no = 0;

static unsigned int last_tcp_id;

static struct tcp_conn *hash_id_conns[TCP_HASH_SIZE];

static struct tcp_conn *hash_ip_conns[TCP_HASH_SIZE];

static gen_lock_t hash_id_lock;

static gen_lock_t hash_ip_lock;


#define tcp_hash(_no1,_no2)  ((_no1+_no2)%TCP_HASH_SIZE)

#define tcp_hash_add_unsafe(_conn,_type,_h) \
	do {\
		if (hash_##_type##_conns[_h]==NULL) {\
			_conn->_type##_next=NULL;\
		} else {\
			_conn->_type##_next = hash_##_type##_conns[_h];\
			hash_##_type##_conns[_h]->_type##_prev=_conn;\
		};\
		_conn->_type##_prev = NULL;\
		hash_##_type##_conns[_h] = _conn;\
	}while(0)

#define tcp_hash_rm_unsafe(_conn,_type,_h) \
	do {\
		if (_conn->_type##_next)\
			_conn->_type##_next->_type##_prev = _conn->_type##_prev;\
		if (_conn->_type##_prev)\
			_conn->_type##_prev->_type##_next = _conn->_type##_next;\
		else\
			hash_##_type##_conns[_h] = _conn->_type##_next;\
	}while(0)


static int tcp_timer_routine(void *param);


int init_tcp_conns(void)
{
	/* init conn hash locks */
	if (lock_init(&hash_id_lock)==0){
		LM_ERR("failed to init lock for hash_id\n");
		return -1;
	}
	if (lock_init(&hash_ip_lock)==0){
		LM_ERR("failed to init lock for hash_ip\n");
		return -1;
	}

	/* timer routine for timeouts */
	if (register_timer( tcp_timer_routine, NULL/*param*/, 2 /*interval*/)!=0) {
		LM_ERR("failed to register timer function\n");
		return -1;
	}

	last_tcp_id = rand();

	/* */

	return 0;
}


static inline void free_tcp_conn(struct tcp_conn *conn)
{
	struct tcp_pending_writes *pw;
	struct tcp_pending_writes *pw_next;

	LM_DBG("freeing connection %p (fd=%d)\n",conn,conn->socket);
	/* close the socket */
	if (conn->socket)
		close(conn->socket);
	/* free pending read */
	if (conn->read.msg)
		shm_free(conn->read.msg);
	/* free pending writes */
	for( pw=conn->write.first ; pw ; pw=pw_next ) {
		pw_next = pw->next;
		shm_free(pw);
	}
	lock_destroy(&conn->write_lock);
	shm_free(conn);
	tcp_connections_no--;  //make it atomic - FIXME
}


void remove_tcp_conn(struct tcp_conn *conn, int extra_ref)
{
	unsigned int h;

	/* unlink it asap */
	lock_get( &hash_id_lock );
	h = tcp_hash(conn->id,0);
	tcp_hash_rm_unsafe(conn,id,h);
	conn->ref -= extra_ref;
	lock_release( &hash_id_lock );

	lock_get( &hash_ip_lock );
	h = tcp_hash(conn->rcv.src_ip.u.addr32[0],conn->rcv.src_port);
	tcp_hash_rm_unsafe(conn,ip,h);
	lock_release( &hash_ip_lock );

	/* conn is no longer in hash (cannot be found and aquired), so if ref==0
	 * we can safely free it */
	if (conn->ref==0)
		free_tcp_conn(conn);
}


void unref_tcp_conn(struct tcp_conn *conn)
{
	lock_get(&hash_id_lock);
	conn->ref--;
	LM_DBG("connection %p (fd=%d) gets ref cnt to %d\n",
		conn,conn->socket,conn->ref);
	if (conn->ref==0) {
		lock_release(&hash_id_lock);
		free_tcp_conn(conn);
		return;
	}
	lock_release(&hash_id_lock);
}


void ref_tcp_conn(struct tcp_conn *conn)
{
	lock_get(&hash_id_lock);
	conn->ref++;
	LM_DBG("connection %p (fd=%d) gets ref cnt to %d\n",
		conn,conn->socket,conn->ref);
	lock_release(&hash_id_lock);
}


/* destroy and releases all TCP conns 
 * this function must be called only at shutdown, in a single thread env.*/
void destroy_tcp_conns(void)
{
	struct tcp_conn *conn;
	struct tcp_conn *cnext;
	int h;

	for( h=0 ; h<TCP_HASH_SIZE ; h++) {
		for( conn=hash_id_conns[h]; conn ; conn=cnext ) {
			cnext = conn->id_next;
			/* free the connection */
			free_tcp_conn(conn);
		}
	}

	lock_destroy(&hash_id_lock);
	lock_destroy(&hash_ip_lock);
}



struct tcp_conn* create_tcp_conn( int s, union sockaddr_union *src_su,
						struct socket_info *dst_si, tcp_conn_state init_state)
{
	struct tcp_conn *conn;
	int h;

	if (tcp_connections_no>=tcp_max_connections) {
		LM_ERR("maximum number of connections exceeded: %d/%d\n",
			tcp_connections_no, tcp_max_connections);
		close(s);
		return NULL;
	}
	tcp_connections_no++;  //make it atomic - FIXME

	/* allocate structure */
	conn = (struct tcp_conn*)shm_malloc(sizeof(struct tcp_conn));
	if (conn==NULL){
		LM_ERR("no more pkg memory\n");
		return NULL;
	}
	memset(conn, 0, sizeof(struct tcp_conn));

	/* init lock */
	if (lock_init(&conn->state_lock)==0){
		LM_ERR("init lock failed\n");
		goto error;
	}
	conn->state = init_state;
	conn->timeout = get_ticks() + tcp_lifetime;

	/* src info */
	conn->rcv.src_su = *src_su;
	su2ip_addr(&conn->rcv.src_ip, src_su);
	conn->rcv.src_port=su_getport(src_su);
	/* dst info */
	conn->rcv.bind_address = dst_si;
	conn->rcv.dst_ip = dst_si->address;
	conn->rcv.dst_port = dst_si->port;
	conn->rcv.proto = PROTO_TCP;

	conn->socket = s;

	LM_DBG("new tcp connection with: %s:%d\n",
		ip_addr2a(&conn->rcv.src_ip),conn->rcv.src_port);

	/* add to the hashes */
	lock_get( &hash_id_lock );
	/* refed by the hash */
	conn->ref = 1;
	conn->id = last_tcp_id;
	h = tcp_hash(conn->id,0);
	tcp_hash_add_unsafe(conn,id,h);
	lock_release( &hash_id_lock );

	lock_get( &hash_ip_lock );
	h = tcp_hash(conn->rcv.src_ip.u.addr32[0],conn->rcv.src_port);
	tcp_hash_add_unsafe(conn,ip,h);
	lock_release( &hash_ip_lock );

	return conn;
error:
	shm_free(conn);
	return NULL;
}


struct tcp_conn *search_tcp_conn(unsigned int id, union sockaddr_union *to)
{
	struct tcp_conn *conn;
	struct ip_addr ip;
	unsigned short port;
	unsigned int hash;

	port = su_getport(to);
	su2ip_addr( &ip, to);

	/* if an ID is available, search for it */
	if (id!=0) {
		hash = tcp_hash( id, 0);
		lock_get( &hash_id_lock );
		for( conn=hash_id_conns[hash] ; conn ; conn=conn->id_next ) {
			if (conn->id == id) {
				if (conn->state==TCP_CONN_TERM)
					break;
				/* validate the connection with ip and port! */
				if ( ip_addr_cmp(&ip,&conn->rcv.src_ip) &&
				(port==conn->rcv.src_port || port==conn->port_alias) ) {
					conn->ref++;
					lock_release( &hash_id_lock );
					return conn;
				}
				/* conn id failed to match */
				break;
			}
		}
		lock_release( &hash_id_lock );
		/* conn id not found */
	}

	/* search based on destination information (ip and port) */
	hash = tcp_hash(ip.u.addr32[0],port);
	lock_get( &hash_ip_lock );
	for( conn=hash_ip_conns[hash] ; conn ; conn=conn->ip_next ) {
		if ( conn->state!=TCP_CONN_TERM && ip_addr_cmp(&ip,&conn->rcv.src_ip)
		&& (port==conn->rcv.src_port || port==conn->port_alias) ) {
			/* WARNING - take care, this is the only place where both
			 * locks are taken in the same time - be aware of dead-locks! */
			lock_get( &hash_id_lock );
			conn->ref++;
			lock_release( &hash_id_lock );
			lock_release( &hash_ip_lock );
			return conn;
		}
	}
	lock_release( &hash_ip_lock );

	return NULL;
}


static int resume_write_on_failue(void *ctx)
{
	context_resume(ctx,-1);
	return 0;
}


static inline void resume_pending_writes(struct tcp_conn *conn)
{
	struct tcp_pending_writes *pw;
	struct tcp_pending_writes *pw_next;
	heap_node_t  task;

	for(pw=conn->write.first ; pw ; pw=pw_next ) {
		pw_next = pw->next;
		/* create a new task to resume exec with failure notice */
		task.fd = conn->socket;
		task.flags = 0;
		task.priority = TASK_PRIO_RESUME_EXEC;
		task.cb = (fd_callback*)resume_write_on_failue;
		task.cb_param = (void*)pw->ctx;
		put_task( reactor_out->disp, task);
	}
}


static int tcp_timer_routine( void *param)
{
	struct tcp_conn *conn;
	struct tcp_conn *cnext;
	int now;
	int h;

	now = get_ticks();

	for( h=0 ; h<TCP_HASH_SIZE ; h++) {

		lock_get( &hash_id_lock );

		for( conn=hash_id_conns[h]; conn ; conn=cnext ) {
			cnext = conn->id_next;
			/* check connection timeout */
			if (conn->timeout<=now) {
				/* timeout on connection activity -> if connection is valid,
				 * fire the OUT reactor to resume (with error) the write 
				 * context and the IN reactor to stop the read operation */
				conn->timeout = 0;
				if ( set_conn_state(conn, TCP_CONN_TERM)!=-1 ) {
					LM_DBG("Terminating conn %p (%d) (time=%d)\n",
						conn,conn->socket,now);
					conn->ref++;
					break;
				}
			}
		}

		lock_release( &hash_id_lock );

		if (conn) {
			/* fire OUT reactor */
			fire_fd( reactor_out, conn->socket);
			/* fire IN reactor */
			fire_fd( reactor_in, conn->socket);
			remove_tcp_conn(conn, 1);
			/* force checking again the same hash */
			h--;
		}
	}

	return 0;
}


/*
 * Returns:
 *    0 - change can be performed
 *   -1 - cannot change
 *    1 - queueing required as other op is in progress (lock is kept open)
 */
int set_conn_state(struct tcp_conn *conn, tcp_conn_state new_state)
{
	lock_get( &conn->state_lock );

	switch (conn->state) {
		case TCP_CONN_READY:
			switch (new_state) {
				case TCP_CONN_WRITING:
				case TCP_CONN_TERM:
					/* operation can be done */
					conn->state = new_state;
					lock_release( &conn->state_lock );
					return 0;
				default :
					lock_release( &conn->state_lock );
					return -1;
			}
			break;
		case TCP_CONN_WRITING:
			switch (new_state) {
				case TCP_CONN_WRITING:
					/* leave lock open for queueing */
					return 1;
				case TCP_CONN_TERM:
					conn->state = new_state;
					lock_release( &conn->state_lock );
					/* push all pending writes back to execution */
					resume_pending_writes(conn);
					return 0;
				case TCP_CONN_READY:
					if (conn->write.first==NULL) {
						conn->state = new_state;
						lock_release( &conn->state_lock );
						return 0;
					} else {
						/* still have queued writes -> leave lock open */
						return 1;
					}
				default :
					lock_release( &conn->state_lock );
					return -1;
			}
			break;
		case TCP_CONN_TERM:
			lock_release( &conn->state_lock );
			return -1;
	}
	return -1;
}





