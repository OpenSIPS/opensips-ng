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
 *  2010-01-xx  created (bogdan)
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "log.h"


void init_random(void)
{
	unsigned int seed;
	//int ur_fd;

	/* to seed the generator for random numbers, try to generate a 
	   pseudo random seed by reading from /dev/random and using 
	   process id and current time*/

	seed=0;

	/* FIXME - does not work on BSD
	if ( (ur_fd=open("/dev/urandom", O_RDONLY))==-1 ) {
		LM_WARN("failed to open /dev/urandom (%d)\n", errno);
		LM_WARN("using a unsafe seed for the pseudo random number generator");
	} else {
		do {
			if (read(ur_fd, (void*)&seed, sizeof(seed))==-1) {
				if (errno==EINTR) continue;
				LM_WARN("failed to read from /dev/urandom (%d)\n", errno);
				return;
			}
		}while(errno);
		LM_DBG("token %u read from /dev/urandom\n", seed);
		close(ur_fd);
	} */
	seed += getpid() + time(0);
	LM_DBG("seeding rand generator with %u\n", seed);
	srand(seed);
}
