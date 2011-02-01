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
 * History:
 * --------
 */



#ifndef mem_h
#define mem_h
#include "../log.h"

/* fix debug defines, DBG_F_MALLOC <=> DBG_QM_MALLOC */
#ifdef F_MALLOC
	#ifdef DBG_F_MALLOC
		#ifndef DBG_QM_MALLOC
			#define DBG_QM_MALLOC
		#endif
	#elif defined(DBG_QM_MALLOC)
		#define DBG_F_MALLOC
	#endif
#endif

/* tranzition mapping TODO */
#define pkg_malloc shm_malloc
#define pkg_free   shm_free


#ifdef F_MALLOC
	/* Fast malloc is used */
#elif  QM_MALLOC || DBG_QM_MALLOC
	/* QM malloc is used */
#elif VQ_MALLOC
	/* QM malloc is used */
#else
	/* system malloc is used */
	#include <stdlib.h>
	void *sys_malloc(size_t, const char *, const char *, int);
	void *sys_realloc(void *, size_t, const char *, const char *, int);
	void sys_free(void *, const char *, const char *, int);

	#define SYSTEM_MALLOC
	#define shm_malloc(s) sys_malloc((s), __FILE__, __FUNCTION__, __LINE__)
	#define shm_realloc(ptr, s) sys_realloc((ptr), (s), __FILE__, __FUNCTION__, __LINE__)
	#define shm_free(p) sys_free((p), __FILE__, __FUNCTION__, __LINE__)
	#define shm_status()
	#define init_shm_mallocs()
	#define MY_PKG_GET_SIZE()
	#define MY_PKG_GET_USED()
	#define MY_PKG_GET_RUSED()
	#define MY_PKG_GET_MUSED()
	#define MY_PKG_GET_FREE()
	#define MY_PKG_GET_FRAGS()
#endif


#ifndef SYSTEM_MALLOC
	#include "shm_mem.h"
	int init_sh_memory(unsigned long size);
#else
	#define init_sh_memory(_c)  0
#endif


#endif
