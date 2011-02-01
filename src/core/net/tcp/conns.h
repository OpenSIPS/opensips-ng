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

#ifndef _CORE_TCP_CONNS_H
#define _CORE_TCP_CONNS_H

#include "../../locking/locking.h"
#include "../../parser/msg_parser.h"
#include "../socket.h"

typedef enum {TCP_CONN_READY=1,
			  TCP_CONN_WRITING,
			  TCP_CONN_TERM
} tcp_conn_state;


struct tcp_pending_writes {
	void *ctx;
	char * buf;
	unsigned int len;
	struct tcp_pending_writes *next;
};

struct tcp_conn {
	/* connection ID - stored in replies and used for faster search */
	unsigned int id;
	/* lock for "test & set" ops on the connection state */
	gen_lock_t state_lock;
	/* state of the connection */
	tcp_conn_state state;
	unsigned int timeout;
	/* ref conter - how many threads are currently using this connection */
	volatile int ref;
	/* network  info */
	int socket;
	struct receive_info rcv;
	unsigned short port_alias;
	/* read info - what was read so far */
	struct _in_data {
		struct sip_msg *msg;
		unsigned int msg_len;
		unsigned int size;
	}read;
	/* write info - pending writes */
	struct _out_data {
		struct tcp_pending_writes active;
		unsigned int offset;
		struct tcp_pending_writes *first;
		struct tcp_pending_writes *last;
	}write;
	/* linking in the hash tables (ID based and IP based) */
	struct tcp_conn *id_next;
	struct tcp_conn *id_prev;
	struct tcp_conn *ip_next;
	struct tcp_conn *ip_prev;
};


#define lock_tcp_conn(_conn)  \
		lock_get(&_conn->state_lock)


#define unlock_tcp_conn(_conn)  \
		lock_release(&_conn->state_lock)


int init_tcp_conns(void);


void destroy_tcp_conns(void);


void remove_tcp_conn(struct tcp_conn *conn, int extra_refs);


void unref_tcp_conn(struct tcp_conn *conn);


void ref_tcp_conn(struct tcp_conn *conn);


struct tcp_conn* create_tcp_conn( int s, union sockaddr_union *src_su,
		struct socket_info *dst_si, tcp_conn_state init_state);


struct tcp_conn *search_tcp_conn(unsigned int id,
		union sockaddr_union *to);


/*
 * Tries to set a new state for a TCP connection
 * Returns:
 *    0 - new state was set
 *   -1 - bogus new state
 *    1 - queueing required as other op is in progress (unloked)
 */
int set_conn_state(struct tcp_conn *conn, tcp_conn_state new_state);


/*
 * adds a new pending write to the transaction
 * WARNING: requires external locking over the connection
 * Returns:
 *    0 - ok
 *   -1 - failure (shm)
 */
static inline int conn_add_pending_write(struct tcp_conn *conn, char * buf,
												unsigned int len, void *ctx)
{
	struct tcp_pending_writes *added;

	LM_DBG("pending write on conn %p (%d)\n", conn, conn->id);

	added = (struct tcp_pending_writes*)shm_malloc
					(sizeof(struct tcp_pending_writes));
	if (added==NULL) {
		LM_ERR("no more shm memory\n");
		return -1;
	}

	added->ctx = ctx;
	added->buf = buf;
	added->len = len;

	added->next = NULL;
	if (conn->write.first==NULL) {conn->write.first = added;}
	else {conn->write.last->next = added;}
	conn->write.last = added;

	return 0;
}


/*
 * consumes the next pending write and move it as active write.
 * WARNING: requires external locking over the connection
 */
static inline void conn_upload_next_write(struct tcp_conn *conn)
{
	struct tcp_pending_writes *old;

	LM_DBG("uploading next write on conn %p (%d)\n", conn, conn->id);

	old = conn->write.first;
	conn->write.active = *old;
	conn->write.offset = 0;
	conn->write.first = old->next;
	if (conn->write.first==NULL)
		conn->write.last = NULL;
	shm_free(old);
}

#endif

