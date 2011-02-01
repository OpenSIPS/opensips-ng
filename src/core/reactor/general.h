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
 *  2010-04-xx  created (adragus)
 */


#ifndef _GENERAL_IO_STRUCT
#define _GENERAL_IO_STRUCT

#include <errno.h>
#include <string.h>

#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif

#ifdef HAVE_KQUEUE
#include <sys/types.h> /* needed on freebsd */
#include <sys/event.h>
#include <sys/time.h>
#endif

#ifdef HAVE_DEVPOLL
#include <sys/devpoll.h>
#endif
#ifdef HAVE_SELECT
/* needed on openbsd for select*/
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
/* needed according to POSIX for select*/
#include <sys/select.h>
#endif

#include <sys/poll.h>
#include <fcntl.h>
#include "../log.h"
#include "fd_map.h"

/*********************POLL TYPES SECTION ******************************/

#define CONTROL_SIZE 3

enum poll_types
{
	POLL_NONE, POLL_POLL, POLL_EPOLL,
	POLL_SELECT, POLL_KQUEUE, POLL_DEVPOLL,
	POLL_END
};

/* all the function and vars are defined in io_wait.c */

extern char* poll_method_str[POLL_END];
extern char* poll_support;


enum poll_types choose_poll_method();

/* returns 0 on success, and an error message on error */
char* check_poll_method(enum poll_types poll_method);

char* poll_method_name(enum poll_types poll_method);
enum poll_types get_poll_type(char* s);



/****************************STRUCTURE SECTION *****************************/

#define ADD_FD 0
#define DEL_FD 1
#define FIRE_FD 2


#ifdef HAVE_KQUEUE
#ifndef KQ_CHANGES_ARRAY_SIZE
#define KQ_CHANGES_ARRAY_SIZE 128

#ifdef __OS_netbsd
#define KEV_UDATA_CAST (intptr_t)
#else
#define KEV_UDATA_CAST
#endif

#endif
#endif

void* receive_loop(void * x);

typedef int (*io_func) (struct fd_map, int, void *);

/*! \brief handler structure */
struct io_wait_handler
{
	struct _reactor * rec;
	int type;

#ifdef HAVE_EPOLL
	struct epoll_event* ep_array;
	int epfd; /* epoll ctrl fd */
#endif

#ifdef HAVE_KQUEUE
	struct kevent* kq_array; /* used for the eventlist*/
	int kq_fd;
#endif


#ifdef HAVE_DEVPOLL
	int dpoll_fd;
#endif


#ifdef HAVE_SELECT
	fd_set master_set;
	fd_set out_set;
	int max_fd_select; /* maximum select used fd */
#endif


	/* common stuff for POLL,  and SELECT
	 * since poll support is always compiled => this will always be compiled */
	int control_pipe[2];
	struct fd_map* fd_hash;
	struct pollfd* fd_array;
	int fd_no; /*  current index used in fd_array */
	int max_fd_no; /* maximum fd no, is also the size of fd_array,
						       fd_hash  and ep_array*/
	enum poll_types poll_method;
	int flags;

	/*	io_func handle_io; */
	void * arg;

};

typedef struct io_wait_handler io_wait_h;

/**********************HELPER SECTION *******************************/

/*! \brief get the corresponding fd_map structure pointer */
#define get_fd_map(h, fd)		(&(h)->fd_hash[(fd)])

/*! \brief remove a fd_map structure from the hash;
 * the pointer must be returned by get_fd_map or hash_fd_map
 */
#define unhash_fd_map(pfm)	\
	do{ \
		(pfm)->fd=-1; \
	}while(0)

/*! \brief add a fd_map structure to the fd hash */
static inline struct fd_map* hash_fd_map(io_wait_h* h, int fd, int flags,
		int priority, fd_callback cb, void* cb_param)
{
	h->fd_hash[fd].last_reactor = h->rec;
	h->fd_hash[fd].fd = fd;
	h->fd_hash[fd].flags = flags;
	h->fd_hash[fd].priority = priority;
	h->fd_hash[fd].cb = cb;
	h->fd_hash[fd].cb_param = cb_param;
	return &h->fd_hash[fd];
}

#define set_fd_flags(f,fd) \
	do{		static int flags;\
			flags=fcntl(fd, F_GETFL); \
			if (flags==-1){ \
				LM_ERR("fnctl: GETFL failed:" \
						" %s [%d]\n", strerror(errno), errno); \
				goto error; \
			} \
			if (fcntl(fd, F_SETFL, flags|(f))==-1){ \
				LM_ERR("fnctl: SETFL" \
							" failed: %s [%d]\n", strerror(errno), errno); \
				goto error; \
			} \
	}while(0)


int inline array_fd_del(io_wait_h * h, int fd1, int idx);
void inline array_fd_add(io_wait_h * h, int fd1, int ev);
int inline safe_remove_from_hash(io_wait_h * h, int fd1);
struct fd_map * safe_add_to_hash(io_wait_h * h, int fd,
		int flags, int priority, fd_callback cb, void *cb_param);

int send_fire(io_wait_h *h, int fd);


#endif
