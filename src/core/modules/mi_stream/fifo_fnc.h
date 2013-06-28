/* 
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of a module for opensips, a free SIP server.
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
 * History:
 * --------
 * 2006-09-25: first version (bogdan)
 */



#ifndef _FIFO_FNC_H_
#define _FIFO_FNC_H_

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include "../../log.h"
#include "../../globals.h"

#define FIFO_REPLY_WAIT     80000

int mi_init_fifo_server(char *fifo_name, int mode, int uid, int gid,
		char* fifo_reply_dir);
char *get_reply_filename( char * file, int len );
int mi_open_reply_pipe( char *pipe_name );


/* write len bytes starting from string 
 * onto fd
 */
static inline int mi_fifofd_reply(int fd,char *string,int len)
{
	int nr_written = 0;
	int bytes;

	while (nr_written < len)
	{
		bytes = write(fd,string+nr_written,len-nr_written);
		if (bytes <= 0)
		{
			if ((errno==EINTR)||(errno==EAGAIN)||(errno==EWOULDBLOCK)) 
				continue;
			else
			{
				LM_ERR("write error in mi_fifo_reply\n");
				return -1;
			}
		}

		nr_written += bytes;
	}

	return 0;
}
#endif /* _FIFO_FNC_H */

