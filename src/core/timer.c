/*
 * Copyright (C) 2010 OpenSIPS Project
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
 *  2010-03-26  created (bogdan)
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "log.h"
#include "globals.h"
#include "threading.h"
#include "dispatcher/dispatcher.h"
#include "mem/mem.h"
#include "timer.h"



struct timer_task {
	heap_node_t  task;
	unsigned int interval;
	utime_t runtime;
	struct timer_task *next;
};

static utime_t  jiffies=0;
static utime_t  ujiffies=0;

static struct timer_task *ttasks = NULL;
static struct timer_task *uttasks = NULL;



unsigned int get_ticks(void)
{
	return jiffies;
}


utime_t get_uticks(void)
{
	return ujiffies;
}


static inline struct timer_task* build_ttask( timer_function f, void *param,
														unsigned int interval)
{
	struct timer_task *tt;

	tt = (struct timer_task*)shm_malloc( sizeof(struct timer_task) );
	if (tt==NULL) {
		LM_ERR("no more shm memory\n");
		return NULL;
	}

	tt->task.fd = 0;
	tt->task.flags = 0;
	tt->task.priority = 100;
	tt->task.cb = f;
	tt->task.cb_param = param;
	tt->interval = interval;
	tt->runtime = interval;
	tt->next = NULL;

	return tt;
}


void destroy_timer(void)
{
	struct timer_task *next;

	while (ttasks) {
		next = ttasks->next;
		shm_free( ttasks);
		ttasks = next;
	}

	while (uttasks) {
		next = uttasks->next;
		shm_free( uttasks);
		uttasks = next;
	}
}


int register_timer( timer_function f, void *param, unsigned int interval)
{
	struct timer_task *tt;
	struct timer_task *it;

	tt = build_ttask(f,param,interval);
	if (tt==NULL) {
		LM_ERR("failed to create new timer task\n");
		return -1;
	}

	/* add at the end of the existing list*/
	if (ttasks==NULL) {
		ttasks = tt;
	} else {
		for( it=ttasks; it->next!=NULL ; it=it->next );
		it->next = tt;
	}

	return 0;
}


int register_utimer( timer_function f, void *param, unsigned int interval)
{
	struct timer_task *tt;
	struct timer_task *it;

	tt = build_ttask(f,param,interval);
	if (tt==NULL) {
		LM_ERR("failed to create new utimer task\n");
		return -1;
	}

	/* add at the end of the existing list*/
	if (uttasks==NULL) {
		uttasks = tt;
	} else {
		for( it=uttasks; it->next!=NULL ; it=it->next );
		it->next = tt;
	}

	return 0;
}


static inline void timer_ticker(void)
{
	struct timer_task* t;

	jiffies += TIMER_TICK;

	for ( t=ttasks ; t ; t=t->next){
		if (jiffies>=t->runtime){
			t->runtime = jiffies + t->interval;
			put_task( reactor_in->disp, t->task);
		}
	}
}


static inline void utimer_ticker(void)
{
	struct timer_task* t;

	ujiffies += UTIMER_TICK;

	for ( t=uttasks ; t ; t=t->next){
		if (ujiffies>=t->runtime){
			t->runtime = ujiffies + t->interval;
			put_task( reactor_in->disp, t->task);
		}
	}
}


static void* timer_thread(void *param)
{
	unsigned int multiple;
	unsigned int cnt;
	struct timeval o_tv;
	struct timeval tv;

	if ( (uttasks==NULL) || ((TIMER_TICK*1000000) == UTIMER_TICK) ) {
		o_tv.tv_sec = TIMER_TICK;
		o_tv.tv_usec = 0;
		multiple = 1;
	} else {
		o_tv.tv_sec = UTIMER_TICK / 1000000;
		o_tv.tv_usec = UTIMER_TICK % 1000000;
		multiple = (( TIMER_TICK * 1000000 ) / UTIMER_TICK ) / 1000000;
	}

	LM_DBG("tv = %ld, %ld , m=%d\n",
		o_tv.tv_sec,o_tv.tv_usec,multiple);

	if (uttasks==NULL) {
		for( ; ; ) {
			tv = o_tv;
			select( 0, 0, 0, 0, &tv);
			timer_ticker();
		}

	} else
	if (ttasks==NULL) {
		for( ; ; ) {
			tv = o_tv;
			select( 0, 0, 0, 0, &tv);
			utimer_ticker();
		}

	} else
	if (multiple==1) {
		for( ; ; ) {
			tv = o_tv;
			select( 0, 0, 0, 0, &tv);
			timer_ticker();
			utimer_ticker();
		}

	} else {
		for( cnt=1 ; ; cnt++ ) {
			tv = o_tv;
			select( 0, 0, 0, 0, &tv);
			utimer_ticker();
			if (cnt==multiple) {
				timer_ticker();
				cnt = 0;
			}
		}
	}
	return NULL;
}


int start_timer_thread(void)
{
	if (UTIMER_TICK>TIMER_TICK*1000000) {
		LM_CRIT("UTIMER > TIMER!!\n");
		return -1;
	}

	if ( ((TIMER_TICK*1000000) % UTIMER_TICK)!=0 ) {
		LM_CRIT("TIMER must be multiple of UTIMER!!\n");
		return -1;
	}

	jiffies=0;
	ujiffies=0;

	/* start timer thread */
	if (pt_create_thread( "timer", timer_thread, NULL)<0 ) {
		LM_ERR("failed to start timer thread\n");
		return -1;
	}


	return 0;
}

