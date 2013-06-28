/*
 * Copyright (C) 2006 Voice Sistem SRL
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
 * History:
 * --------
 *  2012-06-06 : Ported to 2.0 and added TCP support ( vlad-paiu )
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "../../modules.h"
#include "../../log.h"
#include "../../daemonize.h"
#include "../../globals.h"
#include "mi_stream.h"
#include "fifo_fnc.h"
#include "tcp_fnc.h"
#include "mi_parser.h"
#include "mi_writer.h"
#include "mi_listener.h"

/* FIFO server vars */
/* FIFO name */
static char *mi_fifo = 0;
/* dir where reply fifos are allowed */
static char *mi_fifo_reply_dir = DEFAULT_MI_REPLY_DIR;
static char *mi_reply_indent = DEFAULT_MI_REPLY_IDENT;
static int  mi_fifo_uid = -1;
static char *mi_fifo_uid_s = 0;
static int  mi_fifo_gid = -1;
static char *mi_fifo_gid_s = 0;
static int  mi_fifo_mode = S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP; /* rw-rw---- */
static int  mi_listen_port = -1;

static int fifo_fd = -1;
static int socket_fd = -1;

int  mod_init(void);
void mod_destroy(void);

static config_param_t module_params[] = {
	{"fifo_name",    &mi_fifo,           PARAM_TYPE_STRING,  0},
	{"fifo_mode",    &mi_fifo_mode,      PARAM_TYPE_INT,     0},
	{"fifo_group",   &mi_fifo_gid_s,     PARAM_TYPE_STRING,  0},
	{"fifo_group",   &mi_fifo_gid,       PARAM_TYPE_INT,     0},
	{"fifo_user",    &mi_fifo_uid_s,     PARAM_TYPE_STRING,  0},
	{"fifo_user",    &mi_fifo_uid,       PARAM_TYPE_INT,     0},
	{"reply_dir",    &mi_fifo_reply_dir, PARAM_TYPE_STRING,  0},
	{"reply_indent", &mi_reply_indent,   PARAM_TYPE_STRING,  0},
	{"listen_port",  &mi_listen_port,	 PARAM_TYPE_INT,	 0},
	{0, 0, 0, 0}
};


struct core_module_interface interface = {
	"mi_stream",
	OPENSIPS_FULL_VERSION,       /* compile version */
	OPENSIPS_COMPILE_FLAGS,      /* compile flags */
	CORE_MODULE_UTILS,
	0,
	/* parameters */
	module_params,
	0,
	/* functions */
	mod_init,
	mod_destroy,
	NULL,
	NULL
};

int mod_init(void)
{
	static fd_wrapper fifo, sock;
	int n;
	struct stat filestat;

	LM_DBG("entering mod_init for mi_fifo\n");
	/* checking the mi_fifo module param */
	if (mi_fifo != NULL && *mi_fifo != 0) {
		LM_DBG("testing fifo existance ...\n");
		n=stat(mi_fifo, &filestat);
		if (n==0){
			/* FIFO exist, delete it (safer) */
			if (unlink(mi_fifo)<0){
				LM_ERR("cannot delete old fifo (%s): %s\n",
					mi_fifo, strerror(errno));
				return -1;
			}
		}else if (n<0 && errno!=ENOENT){
			LM_ERR("FIFO stat failed: %s\n", strerror(errno));
			return -1;
		}

		/* checking the mi_fifo_reply_dir param */
		if(!mi_fifo_reply_dir || *mi_fifo_reply_dir == 0){
			LM_ERR("mi_fifo_reply_dir parameter is empty\n");
			return -1;
		}

		n = stat(mi_fifo_reply_dir, &filestat);
		if(n < 0){
			LM_ERR("directory stat failed: %s\n", strerror(errno));
			return -1;
		}

		if(S_ISDIR(filestat.st_mode) == 0){
			LM_ERR("mi_fifo_reply_dir parameter is not a directory\n");
			return -1;
		}

		/* check mi_fifo_mode */
		if(!mi_fifo_mode){
			LM_WARN("cannot specify mi_fifo_mode = 0, forcing it to rw-------\n");
			mi_fifo_mode = S_IRUSR| S_IWUSR;
		}

		if (mi_fifo_uid_s){
			if (user2uid(&mi_fifo_uid, &mi_fifo_gid, mi_fifo_uid_s)<0){
				LM_ERR("bad user name %s\n", mi_fifo_uid_s);
				return -1;
			}
		}

		if (mi_fifo_gid_s){
			if (group2gid(&mi_fifo_gid, mi_fifo_gid_s)<0){
				LM_ERR("bad group name %s\n", mi_fifo_gid_s);
				return -1;
			}
		}

		fifo_fd = mi_init_fifo_server( mi_fifo, mi_fifo_mode,
			mi_fifo_uid, mi_fifo_gid, mi_fifo_reply_dir);
		if ( fifo_fd < 0 ) {
			LM_ERR("Error initializing the fifo server\n");
			return -1;
		}

		fifo.fd = fifo_fd;
		fifo.type = TYPE_FIFO;
		fifo.current_comm_len = 0;
		fifo.mi_buf_pos = 0;
	}

	if (mi_listen_port >= 0) {

		socket_fd = mi_init_sock_server(mi_listen_port);
		if (socket_fd < 0)
		{
			LM_ERR("Error initializing the socket server\n");
			return -1;
		}

		sock.fd = socket_fd;
		sock.type = TYPE_SOCKET_SERVER;
		sock.current_comm_len = 0;
		sock.mi_buf_pos = 0;
	}

	if (fifo_fd < 0 && socket_fd < 0) {
		LM_ERR("Neither listen port nor fifo file specified!\n");
		return -1;
	}

	if ( mi_writer_init(mi_reply_indent)!=0 ) {
		LM_CRIT("failed to init the reply writer\n");
		return -1;
	}

	if (fifo_fd > 0)
		submit_task(reactor_in,(fd_callback *)mi_listener, &fifo,
				TASK_PRIO_READ_IO,fifo_fd,0);
	if (socket_fd > 0)
		submit_task(reactor_in,(fd_callback *)mi_listener, &sock,
				TASK_PRIO_READ_IO,socket_fd,0);

	return 0;
}

void mod_destroy(void)
{
	int n;
	struct stat filestat;

	if (fifo_fd > 0) {
		/* destroying the fifo file */
		n=stat(mi_fifo, &filestat);
		if (n==0){
			/* FIFO exist, delete it (safer) */
			if (unlink(mi_fifo)<0){
				LM_ERR("cannot delete the fifo (%s): %s\n",
						mi_fifo, strerror(errno));
			}
		} else if (n<0 && errno!=ENOENT)
			LM_ERR("FIFO stat failed: %s\n", strerror(errno));

		close(fifo_fd);
	}
	if (socket_fd > 0)
		close(socket_fd);

	LM_DBG("mi_fifo module destroyed\n");

}



