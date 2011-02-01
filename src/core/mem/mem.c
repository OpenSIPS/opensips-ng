/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of opensips, a free SIP server.
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
 *
 * History:
 * --------
 * 
 */


#include <stdio.h>
#include "../log.h"
#include "mem.h"

#ifndef SYSTEM_MALLOC
	/* using iternal shm malloc */

int init_sh_memory(unsigned long shmem_size)
{
	if (shm_mem_init(shmem_size)<0) {
		LM_CRIT("could not initialize shared memory pool, exiting...\n");
		 fprintf(stderr, "Too much shared memory demanded: %ld\n",
			shmem_size );
		return -1;
	}
	return 0;
}


#else
	/* using system malloc */

void *
sys_malloc(size_t s, const char *file, const char *function, int line)
{
	void *v;

	v = malloc(s);
	LM_DBG("%s:%s:%d: malloc %p size %lu end %p\n", file, function, line,
	    v, (unsigned long)s, (char *)v + s);
	return v;
}

void *
sys_realloc(void *p, size_t s, const char *file, const char *function, int line)
{
	void *v;

	v = realloc(p, s);
	LM_DBG("%s:%s:%d: realloc old %p to %p size %lu end %p\n", file,
	    function, line, p, v, (unsigned long)s, (char *)v + s);
	return v;
}

void
sys_free(void *p, const char *file, const char *function, int line)
{

	LM_DBG("%s:%s:%d: free %p\n", file, function, line, p);
	free(p);
}
#endif
