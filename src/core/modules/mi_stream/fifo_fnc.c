/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * ---------
 *  2006-09-25  first version (bogdan)
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../log.h"
#include "../../globals.h"
#include "../../utils.h"
#include "fifo_fnc.h"
#include "mi_stream.h"
#include "mi_parser.h"

static int  mi_fifo_read = 0;
static int  mi_fifo_write = 0;
static char reply_fifo_s[MAX_MI_FILENAME];
static int  reply_fifo_len = 0;

int mi_init_fifo_server(char *fifo_name, int mi_fifo_mode,
						int mi_fifo_uid, int mi_fifo_gid, char* fifo_reply_dir)
{
	/* create FIFO ... */
	if ((mkfifo(fifo_name, mi_fifo_mode)<0)) {
		LM_ERR("can't create FIFO: %s (mode=%d)\n", strerror(errno), mi_fifo_mode);
		return -1;
	}

	LM_DBG("FIFO created @ %s\n", fifo_name );

	if ((chmod(fifo_name, mi_fifo_mode)<0)) {
		LM_ERR("can't chmod FIFO: %s (mode=%d)\n", strerror(errno), mi_fifo_mode);
		return -1;
	}

	if ((mi_fifo_uid!=-1) || (mi_fifo_gid!=-1)){
		if (chown(fifo_name, mi_fifo_uid, mi_fifo_gid)<0){
			LM_ERR("failed to change the owner/group for %s  to %d.%d; %s[%d]\n",
				fifo_name, mi_fifo_uid, mi_fifo_gid, strerror(errno), errno);
			return -1;
		}
	}

	LM_DBG("fifo %s opened, mode=%o\n", fifo_name, mi_fifo_mode );

	/* open it non-blocking or else wait here until someone
	 * opens it for writing */
	mi_fifo_read=open(fifo_name, O_RDONLY|O_NONBLOCK, 0);
	if (mi_fifo_read<0) {
		LM_ERR("mi_fifo_read did not open: %s\n", strerror(errno));
		return -1;
	}

	/* make sure the read fifo will not close */
	mi_fifo_write=open( fifo_name, O_WRONLY|O_NONBLOCK, 0);
	if (mi_fifo_write<0) {
		LM_ERR("fifo_write did not open: %s\n", strerror(errno));
		return -1;
	}

	/* init fifo reply dir buffer */
	reply_fifo_len = strlen(fifo_reply_dir);
	memcpy(reply_fifo_s, fifo_reply_dir, reply_fifo_len);

	return mi_fifo_read;
}

char *get_reply_filename( char * file, int len )
{
	if ( strchr(file,'.') || strchr(file,'/') || strchr(file, '\\') ) {
		LM_ERR("forbidden filename: %s\n", file);
		return 0;
	}

	if (reply_fifo_len + len + 1 > MAX_MI_FILENAME) {
		LM_ERR("reply fifoname too long %d\n",reply_fifo_len + len);
		return 0;
	}

	memcpy( reply_fifo_s+reply_fifo_len, file, len );
	reply_fifo_s[reply_fifo_len+len]=0;

	return reply_fifo_s;
}

/* reply fifo security checks:
 * checks if fd is a fifo, is not hardlinked and it's not a softlink
 * opened file descriptor + file name (for soft link check)
 * returns 0 if ok, <0 if not */
static int mi_fifo_check(int fd, char* fname)
{
	struct stat fst;
	struct stat lst;
	
	if (fstat(fd, &fst)<0){
		LM_ERR("fstat failed: %s\n", strerror(errno));
		return -1;
	}
	/* check if fifo */
	if (!S_ISFIFO(fst.st_mode)){
		LM_ERR("%s is not a fifo\n", fname);
		return -1;
	}
	/* check if hard-linked */
	if (fst.st_nlink>1){
		LM_ERR("security: fifo_check: %s is hard-linked %d times\n", fname, (unsigned)fst.st_nlink);
		return -1;
	}

	/* lstat to check for soft links */
	if (lstat(fname, &lst)<0){
		LM_ERR("lstat failed: %s\n", strerror(errno));
		return -1;
	}
	if (S_ISLNK(lst.st_mode)){
		LM_ERR("security: fifo_check: %s is a soft link\n", fname);
		return -1;
	}
	/* if this is not a symbolic link, check to see if the inode didn't
	 * change to avoid possible sym.link, rm sym.link & replace w/ fifo race
	 */
	if ((lst.st_dev!=fst.st_dev)||(lst.st_ino!=fst.st_ino)){
		LM_ERR("security: fifo_check: inode/dev number differ: %d %d (%s)\n",
			(int)fst.st_ino, (int)lst.st_ino, fname);
		return -1;
	}
	/* success */
	return 0;
}

int mi_open_reply_pipe( char *pipe_name )
{
	int fifofd;
	int retries=FIFO_REPLY_RETRIES;

	if (!pipe_name || *pipe_name==0) {
		LM_DBG("no file to write to about missing cmd\n");
		return -1;
	}

tryagain:
	/* open non-blocking to make sure that a broken client will not 
	 * block the FIFO server forever */
	fifofd=open( pipe_name, O_WRONLY | O_NONBLOCK );
	if (fifofd==-1) {
		/* retry several times if client is not yet ready for getting
		   feedback via a reply pipe
		*/
		if (errno==ENXIO) {
			/* give up on the client - we can't afford server blocking */
			if (retries==0) {
				LM_ERR("no client at %s\n",pipe_name );
				return -1;
			}
			/* don't be noisy on the very first try */
			if (retries!=FIFO_REPLY_RETRIES)
				LM_DBG("retry countdown: %d\n", retries );
			sleep_us( FIFO_REPLY_WAIT );
			retries--;
			goto tryagain;
		}
		/* some other opening error */
		LM_ERR("open error (%s): %s\n", pipe_name, strerror(errno));
		return -1;
	}
	/* security checks: is this really a fifo?, is 
	 * it hardlinked? is it a soft link? */
	if (mi_fifo_check(fifofd, pipe_name)<0) goto error;

	return fifofd;
error:
	close(fifofd);
	return -1;
}

