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

