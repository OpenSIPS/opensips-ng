/*
 * Copyright (C) 2011 OpenSIPS Solutions
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
 *  2012-06-06 created (vlad-paiu)
 */

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../../log.h"

#include "tcp_fnc.h"
#include "mi_stream.h"

int mi_init_sock_server(int port)
{
	struct sockaddr_in stSockAddr;
	int sockfd,flags;

	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0)
    {
		LM_ERR("failed to init socket\n");
		return -1;
	}

	if (-1 == (flags = fcntl(sockfd, F_GETFL, 0)))
		flags = 0;

	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		LM_WARN("failed to set nonblocking mode. give up on socket\n");
		return -1;
	}

	memset(&stSockAddr, 0, sizeof stSockAddr);
	stSockAddr.sin_family = AF_INET;
	stSockAddr.sin_port = htons(port);
	stSockAddr.sin_addr.s_addr = INADDR_ANY;

	if(-1 == bind(sockfd,(struct sockaddr *)&stSockAddr, sizeof(stSockAddr)))
	{
		LM_ERR("failed to bind socket\n");
		close(sockfd);
		return -1;
	}

	if(-1 == listen(sockfd, MAX_TCP_QUEUE))
	{
		LM_ERR("failed to listen on socket\n");
		close(sockfd);
		return -1;
	}

	return sockfd;
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
