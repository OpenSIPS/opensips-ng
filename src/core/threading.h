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




#ifndef _H_CORE_THREADING
#define _H_CORE_THREADING

#include <pthread.h>

/* TSD - Thread Specific Data -  only integer values! */
#ifdef TSD_USE_KEY
	// TODO create ?
	#define declare_tsd(_name)   pthread_key_t  _name
	#define extern_tsd(_name)    extern pthread_key_t  _name
	#define set_tsd(_name,_val)  (void)pthread_setspecific(_name, (void*)_val)
	#define get_tsd(_name)       ((int)(long)pthread_getspecific(_name))
#else
	#define declare_tsd(_name)   __thread long  _name = 0
	#define extern_tsd( _name)   extern __thread long  _name
	#define set_tsd(_name,_val)  _name = _val
	#define get_tsd(_name)       (_name)
#endif

#define MAX_TT_NAME 128

extern_tsd( thread_id ) ;

struct thread_info {
	volatile pthread_t id;
	char name[MAX_TT_NAME];
	void *(*thread_routine)(void*);
	void *thread_arg;
	volatile unsigned char core_dump;
};

#define PT_CORE_NONE      0
#define PT_CORE_WAITING   1
#define PT_CORE_ALLOWED   2

extern int rt_coredump;

extern struct thread_info *tt;


int pt_init_signals(void);

void pt_wait_signals(void(*do_shutdown)(void));

int init_main_thread(char *name);

int pt_create_thread(char *name,
		void *(*start_routine)(void*), void *arg);

void get_threads_info(struct thread_info** ttp, int *np);

#endif

