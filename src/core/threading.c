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



#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>


#include "log.h"
#include "mem/mem.h"

declare_tsd( thread_id );

struct thread_info *tt = NULL;

/* RealTime coredump? if RT, the corefile will be generated not at
   SEG FAULT time, but after shuutdown procedure */
int rt_coredump = 1;

/* how many threads we created so far */
static int threads_counter = 0;

/* here we save the default SEGFAULT handler */
static struct sigaction old_sigsegv_hdl;


static void resume_threads_coredumping(void)
{
	int i;

	/* if threads waiting for core dumping, allow it now */
	for ( i=1 ; i<threads_counter ; i++) {
		if ( tt[i].core_dump==PT_CORE_WAITING ) {
			LM_INFO("thread <%s> (id=%d) is allowed to coredump\n",
				tt[i].name, i);
			tt[i].core_dump = PT_CORE_ALLOWED;
			/* allow thread to proceed with core dump */
			usleep(10);
		}
	}
}


static void sig_handler(int sig, siginfo_t *info, void *p)
{
	int idx;

	idx = get_tsd(thread_id);

	LM_INFO("Thread %d received signal %d\n", idx, sig);

	if ( idx==0 ) {
		/* attendent thread received dealy signall */
		/* before allowing a coredump, check id there are 
		   any other threads waiting to dump a core */
		resume_threads_coredumping();
		/* if still alive, we proceed with core dumping */
	} else if ( !rt_coredump ) {
		LM_INFO("no rt coredump -> sending signal SIGTERM to proccess\n");
		tt[idx].core_dump = PT_CORE_WAITING;
		/* Send SIGTERM to attendent */
		kill(getpid(), SIGTERM);
		/* waiting for cleanup */
		while (tt[idx].core_dump != PT_CORE_ALLOWED)
			usleep(10);
		/*proceed with the core dumping */
	}

	/* Put back old sigaction to get a core dump */
	if (sigaction(SIGSEGV, &old_sigsegv_hdl, NULL) < 0) {
		LM_ERR("failed to reset old sigaction");
		return ;
	}
}


static void terminate_all_threads(void)
{
	int i, ret;

	/* cancel the valid and not-core-dumping threads */
	for( i=1 ; i<threads_counter ; i++ ) {
		if (tt[i].id && tt[i].core_dump==PT_CORE_NONE ) {
			if (pthread_cancel(tt[i].id)!=0) {
				/* failed to cancel thread, do not wait for it*/
				tt[i].id = 0;
				LM_ERR("failed to cancel thread <%s> id=%d\n",
					tt[i].name, i);
			}
		}
	}

	/* wait for cancelled threads to terminate */
	for ( i=1 ; i<threads_counter ; i++) {
		if ( tt[i].id && tt[i].core_dump==PT_CORE_NONE ) {
			ret = pthread_join( tt[i].id, NULL);
			LM_INFO("thread <%s> (id=%d) finished with code %d\n",
				tt[i].name, i, ret);
		}
	}
}


/* wait for ever and consume all signals received by our application
 */
void pt_wait_signals( void(*do_shutdown)(void) )
{
	sigset_t signal_set;
	int sig;

	while(1) {
		/* wait for any and all signals */
		sigfillset( &signal_set );
		sigwait( &signal_set, &sig );

		/* when we get this far, we've
		 * caught a signal */

		LM_INFO("Attendent received signal %d\n",sig);
		switch( sig ) {
			/* exit sigals */
			case SIGQUIT:
			case SIGTERM:
			case SIGINT:
				LM_INFO("stopping all other threads\n");
				/* cancel all other threads */
				terminate_all_threads();
				/* do application cleanup */
				LM_INFO("doing shutdown....\n");
				do_shutdown();
				/* allow pending coredumps */
				resume_threads_coredumping();
				return;

			/* by default ignore signal */
			default:
				;
		}
	}

	return;
}


/* This function creats the signal mask to be inherited by all threads;
 * The mask will block all signalls (to be diverted to the attendend thread)
 * and it will allow only the sync. signals to the forked threads.
 */
int pt_init_signals(void)
{
	sigset_t signal_set;
	struct sigaction act;

	/* set handlers for all sync signals */
	memset (&act, 0, sizeof(act));

	/* Use the sa_sigaction field because the handles has
	 * two additional parameters */
	act.sa_sigaction = &sig_handler;

	/* The SA_SIGINFO flag tells sigaction() to use the sa_sigaction field,
	 * not sa_handler. */
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGSEGV, &act, &old_sigsegv_hdl) < 0) {
		LM_ERR("sigaction(SIGSEGV) failed\n");
		return -1;
	}

	if (sigaction(SIGBUS, &act, NULL) < 0) {
		LM_ERR("sigaction(SIGBUS) failed\n");
		return -1;
	}

	if (sigaction(SIGFPE, &act, NULL) < 0) {
		LM_ERR("sigaction(SIGFPE) failed\n");
		return -1;
	}

	/* block all other signals */
	if (sigfillset( &signal_set )!=0 ) {
		LM_ERR("sigfillset() failed\n");
		return -1;
	}
	if (sigdelset(&signal_set,SIGSEGV)!=0 ) {
		LM_ERR("sigdelset(SIGSEGV) failed\n");
		return -1;
	}
	if (sigdelset(&signal_set,SIGBUS)!=0 ) {
		LM_ERR("sigdelset(SIGBUS) failed\n");
		return -1;
	}
	if (sigdelset(&signal_set,SIGFPE)!=0 ) {
		LM_ERR("sigdelset(SIGFPE) failed\n");
		return -1;
	}

	if (pthread_sigmask( SIG_BLOCK, &signal_set,NULL )!=0) {
		LM_ERR("pthread_sigmask() failed\n");
		return -1;
	}

	return 0;
}



int init_main_thread(char *name)
{
	/* alloca a new entry for it in the thread table - it will have
	 * IDX 0 all the time */
	tt = (struct thread_info*)shm_malloc( sizeof(struct thread_info) );
	if (tt==NULL) {
		LM_ERR("no more shared memory\n");
		return -1;
	}
	memset( tt, 0, sizeof(struct thread_info));

	/* fill in the thread info */
	strncpy( tt[0].name, name, MAX_TT_NAME-1);
	tt[0].id = pthread_self();

	/* set the thread id (index) */
	set_tsd( thread_id, (int)(long)0 );

	threads_counter ++;

	return 0;
}


static void* thread_startup( void *idx)
{
	/* first set the thread id (index) */
	set_tsd( thread_id, (int)(long)idx );

	if (pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL) ) {
		LM_ERR("failed to enable CANCEL\n");
		goto failed;
	}
	if (pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL) ) {
		LM_ERR("failed to set the CANCEL state\n");
		goto failed;
	}

	(void)tt[(long)idx].thread_routine( tt[(long)idx].thread_arg );

	/* if we get here, it means the thread failed to init or the thread
	 * ended - which should not happen */
failed:
	/* mark our thread as done */
	tt[(long)idx].id = 0;
	/* trigger shutdown */
	kill(getpid(), SIGTERM);
	return NULL;
}


/* creats a new thread, set the thread attributes, its name and
 * adds the thread to the Thread Table
 * IMPORTANT: to be called only by attendent!!
 */
int pt_create_thread(char *name,
		void *(*thread_routine)(void*), void *arg)
{
	pthread_t tp;

	/* add a new TT record */
	tt = (struct thread_info*)shm_realloc
		( tt, (threads_counter+1)*sizeof(struct thread_info) );
	if (tt==NULL) {
		LM_ERR("no more shared memory - failed to realloc\n");
		return -1;
	}
	memset( tt+threads_counter, 0, sizeof(struct thread_info));

	/* fill in the thread info */
	strncpy( tt[threads_counter].name, name, MAX_TT_NAME-1);
	tt[threads_counter].thread_routine = thread_routine;
	tt[threads_counter].thread_arg = arg;

	if (pthread_create(&tp,NULL,thread_startup,(void*)(long)threads_counter)<0){
		LM_ERR("failed to create new thread <%s> (counter=%d)\n",
			name, threads_counter);
		return -1;
	}

	tt[threads_counter].id = tp;

	threads_counter ++;

	return 0;
}


void get_threads_info(struct thread_info** ttp, int *np)
{
	*ttp = tt;
	*np = threads_counter;
}
