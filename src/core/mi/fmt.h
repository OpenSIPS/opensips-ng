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
 * History:
 * ---------
 *  2006-09-08  first version (bogdan)
 */

/*!
 * \file 
 * \brief MI :: Format handling
 * \ingroup mi
 */



#ifndef _MI_FMT_H_
#define _MI_FMT_H_

#include <stdarg.h>
#include <errno.h>

#include "../mem/mem.h"
#include "../log.h"

/*! \brief size of the buffer used for printing the FMT */
#define DEFAULT_MI_FMT_BUF_SIZE 512


static inline char* mi_print_fmt(char *fmt, va_list ap, int *len)
{
	int n;
	char *buf;

	buf = (char*)shm_malloc(DEFAULT_MI_FMT_BUF_SIZE);
	if (buf==NULL) {
		LM_ERR("failed to allocate more shm mem\n");
		return NULL;
	}

	n = vsnprintf( buf, DEFAULT_MI_FMT_BUF_SIZE, fmt, ap);
	if (n<0 || n>=DEFAULT_MI_FMT_BUF_SIZE) {
		LM_ERR("formatting failed with n=%d, %s\n",n,strerror(errno));
		return NULL;
	}

	*len = n;
	return buf;
}

#endif
