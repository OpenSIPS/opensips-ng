/*
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
 * Copyright (C) 2013 OpenSIPS Solutions
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
 * ---------
 *  2013-06-28  first version (razvancrainea)
 */


#include <sys/socket.h>
#include <stdlib.h>
#include "mi_listener.h"
#include "mi_writer.h"
#include "mi_stream.h"
#include "mi_parser.h"
#include "fifo_fnc.h"
#include "../../mi/mi.h"
#include "../../utils.h"
#include "../../log.h"
#include "../../globals.h"

#define mi_open_reply(_name,_file,_err) \
	do { \
		_file = mi_open_reply_pipe( _name ); \
		if (_file < 0) { \
			LM_ERR("cannot open reply pipe %s\n", _name); \
			goto _err; \
		} \
	} while(0)


int mi_writer(reactor_t *rec,int fd,void *param)
{
	struct mi_root *tree;
	fd_wrapper *wrap;
	reply_wrapper *response;
		
	response = (reply_wrapper *)param;
	tree = response->reply_tree;
	wrap = response->wrap;

	mi_write_tree(fd,tree,wrap->buffer);
	free_mi_tree(tree);
	shm_free(response);
	/* fifo used because reply fd different from
	 * source fd */
	if (fd != wrap->fd)
		close(fd);

	/* process other MI commands on this fd */
	wrap->current_comm_len = 0;
	wrap->mi_buf_pos = 0;
	submit_task(reactor_in,(fd_callback *)mi_listener,wrap,
			TASK_PRIO_READ_IO,wrap->fd,0);
	return 0;
}



int mi_listener(void *param)
{
	int reply_fd = -1,fd,connect_fd,flags;
	int line_len,bytes_recv,*current_comm_len;
	char *command,*file_sep,*file=NULL,*first_line,*mi_buf;
	struct mi_cmd *f;
	struct mi_root *mi_cmd, *mi_rpl;
	struct mi_handler *hdl;
	fd_wrapper *new_conn;
	reply_wrapper *response;

	fd_wrapper *wrap = (fd_wrapper*)param;

	/* event on the socket listening for new connections */
	if (wrap->type == TYPE_SOCKET_SERVER)
	{
		connect_fd = accept(wrap->fd,NULL,NULL); 
		if(connect_fd < 0)
		{
			if (!(errno == EAGAIN || errno == EWOULDBLOCK || ECONNABORTED ||
					errno == EPROTO || errno == EINTR))
				LM_ERR("Error in accepting new connection\n");
			goto reaccept;
		}

		if (-1 == (flags = fcntl(connect_fd, F_GETFL, 0)))
			flags = 0;

		if (fcntl(connect_fd, F_SETFL, flags | O_NONBLOCK) < 0)
		{
			LM_WARN("failed to set nonblocking mode. give up on socket\n");
			close(connect_fd);
			goto reaccept;
		}

		new_conn = shm_malloc(sizeof(fd_wrapper));
		if (new_conn == NULL)
		{
			LM_ERR("no more shm memory\n");
			goto reaccept;
		}

		new_conn->fd = connect_fd;
		new_conn->type = TYPE_SOCKET_CLIENT;
		new_conn->current_comm_len = 0;
		new_conn->mi_buf_pos = 0;

		submit_task(reactor_in,(fd_callback *)mi_listener,new_conn,
				TASK_PRIO_READ_IO,connect_fd,0);
reaccept:
		submit_task(reactor_in,(fd_callback *)mi_listener,wrap,
			TASK_PRIO_READ_IO,wrap->fd,0);
		return 0;
	}

	/* got here, means we have received part of a command,
	 * either on fifo or TCP
	 */
	current_comm_len = &(wrap->current_comm_len);
	mi_buf = wrap->buffer;
	fd = wrap->fd;

read_command:
	bytes_recv = mi_read_and_append(mi_buf,fd,current_comm_len);
	if (bytes_recv == -1)
	{
		LM_ERR("Error reading from input fifo !\n");
		goto force_exit;
	}
	else if (bytes_recv == -2)
	{
		/* other end closed socket */
		close(wrap->fd);
		shm_free(wrap);
		return 0;
	}
	else if (bytes_recv == -3)
	{
		/* buffer full, give it one last check for validity before flushing */
		if (!mi_is_valid(mi_buf,*current_comm_len))
		{
			*current_comm_len = 0;
			LM_DBG("MI buffer full and still haven't got a valid command\n");
			/* maybe more stuff to read ? */
			goto read_command;
		}
		else 
			goto valid;
	}

	if (!mi_is_valid(mi_buf,*current_comm_len))
	{
		/* go back to reactor
		 * maybe command hasn't been received in one piece
		 */
		submit_task(reactor_in,(fd_callback *)mi_listener,wrap,
				TASK_PRIO_READ_IO,fd,0);
		return 0;
	}

	/* we now have a full valid command */
	/* get first line */
valid:
	first_line = memchr(mi_buf,'\n',MAX_MI_FIFO_BUFFER);
	if (first_line == NULL)
	{
		LM_ERR("Malformed MI command \n");
		goto force_exit;
	}
	line_len = first_line - mi_buf + 1;
	wrap->mi_buf_pos = line_len;
	/* trim from right */
	while(line_len) {
		if(mi_buf[line_len-1]=='\n' || mi_buf[line_len-1]=='\r'
			|| mi_buf[line_len-1]==' ' || mi_buf[line_len-1]=='\t' ) {
			line_len--;
			mi_buf[line_len]=0;
		} else break;
	} 
	
	if (line_len==0) {
		LM_ERR("command empty\n");
		goto force_exit;
	}
	if (line_len<3) {
		LM_ERR("command must have at least 3 chars\n");
		goto force_exit;
	}
	if (*mi_buf!=MI_CMD_SEPARATOR) {
		LM_ERR("command must begin with %c: %.*s\n", MI_CMD_SEPARATOR, line_len, mi_buf );
		goto force_exit;
	}
	command = mi_buf+1;
	file_sep=strchr(command, MI_CMD_SEPARATOR );
	if (file_sep==NULL) {
		LM_ERR("file separator missing\n");
		goto force_exit;
	}
	if (file_sep==command) {
		LM_ERR("empty command\n");
		goto force_exit;
	}
	if (*(file_sep+1)==0) {
		/* tcp MI commands should not specify a reply fifo */
		if (wrap->type == TYPE_SOCKET_CLIENT)
			reply_fd = fd;
		else
		{
			/* have fifo MI command and no reply fifo provided */
			LM_ERR("No reply fifo provided !\n");
			goto force_exit;
		}
	} else {
		/* maybe tcp client also specifies a reply fifo ?
		 * read and ignore it */
		file = file_sep+1;
		file = get_reply_filename(file, mi_buf+line_len-file);
		if (file==NULL) {
			LM_ERR("trimming filename\n");
			goto force_exit;
		}
		if (wrap->type == TYPE_SOCKET_CLIENT)
			reply_fd = fd;
	}

	/* make command zero-terminated */
	*file_sep=0;

	f=lookup_mi_cmd( command, strlen(command) );
	if (f==0) {
		LM_ERR("command %s is not available\n", command);
		if (reply_fd < 0)
			mi_open_reply( file, reply_fd, force_exit);

		mi_fifofd_reply(reply_fd,"500 command '",strlen("500 command '"));
		mi_fifofd_reply(reply_fd,command,strlen(command));
		mi_fifofd_reply(reply_fd,"' not available\n",strlen("' not available\n"));
		goto close_fd;
	}

//	/* if asyncron cmd, build the async handler */
//	if (f->flags&MI_ASYNC_RPL_FLAG) {
//		hdl = build_async_handler( file, strlen(file) );
//		if (hdl==0) {
//			LM_ERR("failed to build async handler\n");
//			mi_open_reply( file, reply_stream, consume1);
//			mi_fifo_reply( reply_stream, "500 Internal server error\n");
//			goto consume2;
//		}
//	} else {
	hdl = 0;
	if (reply_fd < 0)
		mi_open_reply( file, reply_fd, force_exit);
//	}

	if (f->flags&MI_NO_INPUT_FLAG) {
		mi_cmd = 0;
	} else {
		mi_cmd = mi_parse_tree(wrap);
		if (mi_cmd==NULL){
			LM_ERR("error parsing MI tree\n");
			mi_fifofd_reply(reply_fd,"400 parse error in command : ",strlen("400 parse error in command : "));
			mi_fifofd_reply(reply_fd,command,strlen(command));
			goto close_fd;
		}
		mi_cmd->async_hdl = hdl;
	}
	
	if ( (mi_rpl=run_mi_cmd(f, mi_cmd,
	(mi_flush_f *)mi_flush_tree, wrap))==0 ) {
		mi_fifofd_reply(reply_fd,"500 command '",strlen("500 command '"));
		mi_fifofd_reply(reply_fd,command,strlen(command));
		mi_fifofd_reply(reply_fd,"' failed\n",strlen("' failed\n"));

		LM_ERR("command (%s) processing failed\n", command );
		if (mi_cmd) 
			free_mi_tree( mi_cmd );
		goto close_fd;
	} else {
		response = shm_malloc(sizeof(reply_wrapper));
		if (response == NULL)
		{
			LM_ERR("no more shm memory\n");
			goto failure;
		}
		
		response->wrap = wrap;
		response->reply_tree = mi_rpl;

		/* if had something to parse, free command now
		 * reply tree will be freed when mi_writer callback 
		 * is called
		 */
		if (mi_cmd)
			free_mi_tree(mi_cmd);

		submit_task(reactor_out,(fd_callback *)mi_writer,response,
				TASK_PRIO_READ_IO,reply_fd,CALLBACK_COMPLEX_F);
		return 0;
	}

failure:
	/* destroy request tree */
	if (mi_cmd) free_mi_tree( mi_cmd );
	/* destroy the reply tree */
	if (mi_rpl) free_mi_tree(mi_rpl);
close_fd:
	/* if TCP MI command, don't close socket
	 * let client close it on the other side
	 * when finished sending commands
	 */
	if (wrap->type == TYPE_FIFO && reply_fd > 0)
		close(reply_fd);
force_exit:
	*current_comm_len = 0;
	wrap->mi_buf_pos = 0;
	submit_task(reactor_in,(fd_callback *)mi_listener,wrap,
			TASK_PRIO_READ_IO,fd,0);
	return 0;
}


/* sets dest to point to the next line from the source buffer
 * in the limit of max characters, and updates the read value
 */
int mi_get_next_line(char **dest,int max,char *source,int *read)
{
	char *pos = source;
	int curr = 0;
	while ((curr < (max-1)) && (*pos != '\n'))
	{
		curr++;
		pos++;
	}
	
	if (curr == max-1)
	{
		LM_ERR("failed getting next line from MI command\n");
		return -1;
	}
	else
	{
		*dest = source;
	}

	/* include \n , just like fgets */
	*read = curr+1;
	return 0;
}

/*
 * reads in buffer b, from fd and updates current_comm_len
 * positive return code means read succeded
 * -1 means error in read
 * -2 means socket got closed
 * -3 means buffer is full
 */
int mi_read_and_append(char *b,int fd,int *current_comm_len)
{
	int bytes_read = 0;
	int original_len = *current_comm_len;
	int nr;

	while (*current_comm_len != MAX_MI_FIFO_BUFFER)
	{
		nr = read(fd,b+bytes_read+original_len,
				MAX_MI_FIFO_BUFFER-*current_comm_len);
		if (nr > 0)
		{
			bytes_read +=nr;
			*current_comm_len +=nr;
			continue;
		}
		else if (nr == 0)
		{
			/* signal received before anything read,
			 * not an error, Otherwise, 0 means other end
			 * closed fd */
			if (errno == EINTR)
				continue;
			else
				return -2;
		}
		else
		{
			if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
				/* no more stuff to read */
				return bytes_read;
			else
			{
				*current_comm_len = original_len;
				LM_ERR("Error in read. errno = %s\n",strerror(errno));
				return -1;
			}
		}
	}

	/* buffered filled */
	return -3;
}

/* check that buffer b of len bytes
 * ends with \n\n sequence
 */
int mi_is_valid(char *b,int len)
{
	if (len > 1)
	{
		if (b[len-2] == '\n' && b[len-1] == '\n')
			return 1;
		else
			return 0;
	}
	
	return 0;
}
//
//
//static inline void free_async_handler( struct mi_handler *hdl )
//{
//	if (hdl)
//		shm_free(hdl);
//}
//
//
//static void fifo_close_async( struct mi_root *mi_rpl, struct mi_handler *hdl,
//																	int done)
//{
//	FILE *reply_stream;
//	char *name;
//
//	name = (char*)hdl->param;
//
//	if ( mi_rpl!=0 || done ) {
//		/*open fifo reply*/
//		reply_stream = mi_open_reply_pipe( name );
//		if (reply_stream==NULL) {
//			LM_ERR("cannot open reply pipe %s\n", name );
//			return;
//		}
//
//		if (mi_rpl!=0) {
//			mi_write_tree( reply_stream, mi_rpl);
//			free_mi_tree( mi_rpl );
//		} else {
//			mi_fifo_reply( reply_stream, "500 command failed\n");
//		}
//
//		fclose(reply_stream);
//	}
//
//	if (done)
//		free_async_handler( hdl );
//	return;
//}
//
//
//static inline struct mi_handler* build_async_handler( char *name, int len)
//{
//	struct mi_handler *hdl;
//	char *p;
//
//	hdl = (struct mi_handler*)shm_malloc( sizeof(struct mi_handler) + len + 1);
//	if (hdl==0) {
//		LM_ERR("no more shared memory\n");
//		return 0;
//	}
//
//	p = (char*)(hdl) + sizeof(struct mi_handler);
//	memcpy( p, name, len+1 );
//
//	hdl->handler_f = fifo_close_async;
//	hdl->param = (void*)p;
//
//	return hdl;
//}
