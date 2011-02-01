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
 *  2003-03-12  split shm_mem_init in shm_getmem & shm_mem_init_mallocs
 *               (andrei)
 *  2004-07-27  ANON mmap support, needed on darwin (andrei)
 *  2004-09-19  shm_mem_destroy: destroy first the lock & then unmap (andrei)
 */



#include <stdlib.h>

#include "shm_mem.h"

#ifndef SYSTEM_MEMORY


#ifdef STATISTICS
stat_export_t shm_stats[] = {
	{"total_size" ,     STAT_IS_FUNC,    (stat_var**)shm_get_size     },
	{"used_size" ,      STAT_IS_FUNC,    (stat_var**)shm_get_used     },
	{"real_used_size" , STAT_IS_FUNC,    (stat_var**)shm_get_rused    },
	{"max_used_size" ,  STAT_IS_FUNC,    (stat_var**)shm_get_mused    },
	{"free_size" ,      STAT_IS_FUNC,    (stat_var**)shm_get_free     },
	{"fragments" ,      STAT_IS_FUNC,    (stat_var**)shm_get_frags    },
	{0,0,0}
};
#endif



gen_lock_t* mem_lock=0;

static void* shm_mempool=(void*)-1;
#ifdef VQ_MALLOC
	struct vqm_block* shm_block;
#elif F_MALLOC
	struct fm_block* shm_block;
#else
	struct qm_block* shm_block;
#endif


inline static void* sh_realloc(void* p, unsigned int size)
{
	void *r;
	shm_lock(); 
	shm_free_unsafe(p);
	r=shm_malloc_unsafe(size);
	shm_unlock();
	return r;
}

/* look at a buffer if there is perhaps enough space for the new size
   (It is beneficial to do so because vq_malloc is pretty stateful
    and if we ask for a new buffer size, we can still make it happy
    with current buffer); if so, we return current buffer again;
    otherwise, we free it, allocate a new one and return it; no
    guarantee for buffer content; if allocation fails, we return
    NULL
*/

#ifdef DBG_QM_MALLOC
void* _shm_resize( void* p, unsigned int s, const char* file, const char* func,
							int line)
#else
void* _shm_resize( void* p , unsigned int s)
#endif
{
#ifdef VQ_MALLOC
	struct vqm_frag *f;
#endif
	if (p==0) {
		LM_DBG("resize(0) called\n");
		return shm_malloc( s );
	}
#	ifdef DBG_QM_MALLOC
#	ifdef VQ_MALLOC
	f=(struct  vqm_frag*) ((char*)p-sizeof(struct vqm_frag));
	LM_DBG("params (%p, %d), called from %s: %s(%d)\n",  
			p, s, file, func, line);
	VQM_DEBUG_FRAG(shm_block, f);
	if (p>(void *)shm_block->core_end || p<(void*)shm_block->init_core){
		LM_CRIT("bad pointer %p (out of memory block!) - aborting\n", p);
		abort();
	}
#endif
#	endif
	return sh_realloc( p, s ); 
}





static int shm_getmem(unsigned long shmem_size)
{
	shm_mempool = (void*)malloc(shmem_size);
	if (shm_mempool==NULL) {
		LM_CRIT("failed to allocated share mem chunck (%ld)\n",shmem_size);
		return -1;
	}
	return 1;
}



static int shm_mem_init_mallocs(void* mempool, unsigned long pool_size)
{
	/* init it for malloc*/
	shm_block=shm_malloc_init(mempool, pool_size);
	if (shm_block==0){
		LM_CRIT("could not initialize shared malloc\n");
		shm_mem_destroy(pool_size);
		return -1;
	}
	mem_lock=shm_malloc_unsafe(sizeof(gen_lock_t)); /* skip lock_alloc, 
													   race cond*/
	if (mem_lock==0){
		LM_CRIT("could not allocate lock\n");
		shm_mem_destroy(pool_size);
		return -1;
	}
	if (lock_init(mem_lock)==0){
		LM_CRIT("could not initialize lock\n");
		shm_mem_destroy(pool_size);
		return -1;
	}
	
	LM_DBG("success\n");
	
	return 0;
}


int shm_mem_init(unsigned long shmem_size)
{
	int ret;
	
	ret=shm_getmem(shmem_size);
	if (ret<0) return ret;
	return shm_mem_init_mallocs(shm_mempool, shmem_size);
}


void shm_mem_destroy(unsigned long shmem_size)
{
	LM_DBG("\n");
	if (mem_lock){
		LM_DBG("destroying the shared memory lock\n");
		lock_destroy(mem_lock); /* we don't need to dealloc it*/
	}
	if (shm_mempool && (shm_mempool!=(void*)-1)) {
		free(shm_mempool);
		shm_mempool=(void*)-1;
	}
}


#endif
