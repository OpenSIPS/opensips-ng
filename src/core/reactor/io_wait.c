/* 
 * Copyright (C) 2005 iptelorg GmbH
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
 *  2005-06-15  created by andrei
 *  2005-06-26  added kqueue (andrei)
 *  2005-07-04  added /dev/poll (andrei)
 */

/*!
 * \file
 * \brief OpenSIPS TCP IO wait common functions
 * Used by tcp_main.c and tcp_read.c
 */

#include <stdlib.h>


#ifdef HAVE_EPOLL
#include <unistd.h> /* close() */
#endif
#ifdef HAVE_DEVPOLL
#include <sys/types.h> /* open */
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h> /* close, ioctl */
#endif

#include <sys/utsname.h> /* uname() */
#include <stdlib.h> /* strtol() */
#include "io_wait.h"
#include "../mem/mem.h"
#include "select.h"

#define local_malloc shm_malloc
#define local_free shm_free

char* poll_support = "poll"
#ifdef HAVE_EPOLL
		", epoll"
#endif
#ifdef HAVE_SELECT
		", select"
#endif
#ifdef HAVE_KQUEUE
		", kqueue"
#endif
#ifdef HAVE_DEVPOLL
		", /dev/poll"
#endif
		;

/*! supported poll methods */
char* poll_method_str[POLL_END] = {"none", "poll", "epoll",
	"select", "kqueue", "/dev/poll"};


#ifdef HAVE_DEVPOLL

/*!
 * \brief /dev/poll specific init
 * \param h IO handle
 * \return -1 on error, 0 on success */
static int init_devpoll(io_wait_h* h)
{
again:
	h->dpoll_fd = open("/dev/poll", O_RDWR);
	if (h->dpoll_fd == -1)
	{
		if (errno == EINTR) goto again;
		LM_ERR("open: %s [%d]\n",
			strerror(errno), errno);
		return -1;
	}
	return 0;
}

/*!
 * \brief dev/poll specific destroy
 * \param h IO handle
 */
static void destroy_devpoll(io_wait_h* h)
{
	if (h->dpoll_fd != -1)
	{
		close(h->dpoll_fd);
		h->dpoll_fd = -1;
	}
}
#endif

/*!
 * \brief return system version
 * Return system version (major.minor.minor2) as (major<<16)|(minor)<<8|(minor2)
 * (if some of them are missing, they are set to 0)
 * if the parameters are not null they are set to the coresp. part 
 * \param major major version
 * \param minor minor version
 * \param minor2 minor2 version
 * \return (major<<16)|(minor)<<8|(minor2)
 */
static unsigned int get_sys_version(int* major, int* minor, int* minor2)
{
	struct utsname un;
	int m1;
	int m2;
	int m3;
	char* p;

	memset(&un, 0, sizeof (un));
	m1 = m2 = m3 = 0;
	/* get sys version */
	uname(&un);
	m1 = strtol(un.release, &p, 10);
	if (*p == '.')
	{
		p++;
		m2 = strtol(p, &p, 10);
		if (*p == '.')
		{
			p++;
			m3 = strtol(p, &p, 10);
		}
	}
	if (major) *major = m1;
	if (minor) *minor = m2;
	if (minor2) *minor2 = m3;
	return ((m1 << 16) | (m2 << 8) | (m3));
}

/*!
 * \brief Check preferred OS poll method
 * \param poll_method supported IO poll methods
 * \return 0 on success, and an error message on error
 */
char* check_poll_method(enum poll_types poll_method)
{
	char* ret;
	unsigned int os_ver;

	ret = 0;
	os_ver = get_sys_version(0, 0, 0);
	switch (poll_method)
	{
	case POLL_NONE:
		break;
	case POLL_POLL:
		/* always supported */
		break;
	case POLL_SELECT:
		/* should be always supported */
#ifndef HAVE_SELECT
		ret = "select not supported, try re-compiling with -DHAVE_SELECT";
#endif
		break;
	case POLL_EPOLL:
	
#ifndef HAVE_EPOLL
		ret = "epoll not supported, try re-compiling with -DHAVE_EPOLL";
#else
		/* only on 2.6 + */
		if (os_ver < 0x020542) /* if ver < 2.5.66 */
			ret = "epoll not supported on kernels < 2.6";
#endif
		break;
	case POLL_KQUEUE:
#ifndef HAVE_KQUEUE
		ret = "kqueue not supported, try re-compiling with -DHAVE_KQUEUE";
#else
		/* only in FreeBSD 4.1, NETBSD 2.0, OpenBSD 2.9, Darwin */
#ifdef __OS_freebsd
		if (os_ver < 0x0401) /* if ver < 4.1 */
			ret = "kqueue not supported on FreeBSD < 4.1";
#elif defined (__OS_netbsd)
		if (os_ver < 0x020000) /* if ver < 2.0 */
			ret = "kqueue not supported on NetBSD < 2.0";
#elif defined (__OS_openbsd)
		if (os_ver < 0x0209) /* if ver < 2.9 ? */
			ret = "kqueue not supported on OpenBSD < 2.9 (?)";
#endif /* assume that the rest support kqueue ifdef HAVE_KQUEUE */
#endif
		break;
	case POLL_DEVPOLL:
#ifndef HAVE_DEVPOLL
		ret = "/dev/poll not supported, try re-compiling with"
				" -DHAVE_DEVPOLL";
#else
		/* only in Solaris >= 7.0 (?) */
#ifdef __OS_solaris
		if (os_ver < 0x0507) /* ver < 5.7 */
			ret = "/dev/poll not supported on Solaris < 7.0 (SunOS 5.7)";
#endif
#endif
		break;

	default:
		ret = "unknown not supported method";
	}
	return ret;
}

/*!
 * \brief Choose a IO poll method
 * \return the choosen poll method
 */
enum poll_types choose_poll_method(void)
{
	enum poll_types poll_method;
	unsigned int os_ver;

	os_ver = get_sys_version(0, 0, 0);
	poll_method = 0;
#ifdef HAVE_EPOLL
	if (os_ver >= 0x020542) /* if ver >= 2.5.66 */
		poll_method = POLL_EPOLL; /* or POLL_EPOLL_ET */

#endif
#ifdef HAVE_KQUEUE
	if (poll_method == 0)
		/* only in FreeBSD 4.1, NETBSD 2.0, OpenBSD 2.9, Darwin */
#ifdef __OS_freebsd
		if (os_ver >= 0x0401) /* if ver >= 4.1 */
#elif defined (__OS_netbsd)
		if (os_ver >= 0x020000) /* if ver >= 2.0 */
#elif defined (__OS_openbsd)
		if (os_ver >= 0x0209) /* if ver >= 2.9 (?) */
#endif /* assume that the rest support kqueue ifdef HAVE_KQUEUE */
			poll_method = POLL_KQUEUE;
#endif
#ifdef HAVE_DEVPOLL
#ifdef __OS_solaris
	if (poll_method == 0)
		/* only in Solaris >= 7.0 (?) */
		if (os_ver >= 0x0507) /* if ver >=SunOS 5.7 */
			poll_method = POLL_DEVPOLL;
#endif
#endif

	if (poll_method == 0) poll_method = POLL_POLL;
	return poll_method;
}

/*!
 * \brief output the IO poll method name
 * \param poll_method used poll method
 */
char* poll_method_name(enum poll_types poll_method)
{
	if (poll_method < POLL_END)
		return poll_method_str[poll_method];
	else
		return "invalid poll method";
}

/*!
 * \brief converts a string into a poll_method
 * \param s converted string
 * \return POLL_NONE (0) on error, else the corresponding poll type 
 */
enum poll_types get_poll_type(char* s)
{
	int r;
	unsigned int l;

	l = strlen(s);
	for (r = POLL_END - 1; r > POLL_NONE; r--)
		if ((strlen(poll_method_str[r]) == l) &&
			(strncasecmp(poll_method_str[r], s, l) == 0))
			break;
	return r;
}

/*!
 * \brief initializes the static vars/arrays
 * \param  h - pointer to the io_wait_h that will be initialized
 * \param  max_fd - maximum allowed fd number
 * \param  poll_method - poll method (0 for automatic best fit)
 */
int init_io_wait(struct _reactor *r, io_wait_h* h, int max_fd, enum poll_types poll_method,int type)
{
	char * poll_err;

	memset(h, 0, sizeof (*h));

	h->rec = r;
	h->type = type;
	h->max_fd_no = max_fd;

	#ifdef HAVE_EPOLL
	h->epfd = -1;
	#endif

	#ifdef HAVE_KQUEUE
	h->kq_fd = -1;
	#endif

	#ifdef HAVE_DEVPOLL
	h->dpoll_fd = -1;
	#endif


	poll_err = check_poll_method(poll_method);

	/* set an appropiate poll method */
	if (poll_err || (poll_method == 0))
	{
		poll_method = choose_poll_method();
		if (poll_err)
		{
			LM_ERR("%s, auto detecting poll method\n", poll_err);
		} else
		{
			LM_INFO("auto detecting poll method\n");
		}
	}

	LM_INFO("using %s as the io watch method\n", poll_method_str[poll_method]);

	h->poll_method = poll_method;

	if (pipe(h->control_pipe) == -1)
	{
		LM_CRIT("could not open pipe for reactor\n");
		goto error;
	}

	/* common stuff, everybody has fd_hash */
	h->fd_hash = local_malloc(sizeof (*(h->fd_hash)) * h->max_fd_no);

	if (h->fd_hash == 0)
	{
		LM_CRIT("could not alloc fd hashtable (%ld bytes)\n",
				(long) sizeof (*(h->fd_hash)) * h->max_fd_no);
		goto error;
	}

	memset((void*) h->fd_hash, 0, sizeof (*(h->fd_hash)) * h->max_fd_no);
	
	switch (poll_method)
	{
	case POLL_POLL:

	#ifdef HAVE_SELECT
	case POLL_SELECT:
	#endif

	#ifdef HAVE_DEVPOLL
	case POLL_DEVPOLL:
	#endif
		h->fd_array = local_malloc(sizeof (*(h->fd_array)) * h->max_fd_no);
		if (h->fd_array == 0)
		{
			LM_CRIT("could not alloc fd array (%ld bytes)\n",
					(long) sizeof (*(h->fd_hash)) * h->max_fd_no);
			goto error;
		}
		memset((void*) h->fd_array, 0, sizeof (*(h->fd_array)) * h->max_fd_no);



		#ifdef HAVE_DEVPOLL
		if ((poll_method == POLL_DEVPOLL) && (init_devpoll(h) < 0))
		{
			LM_CRIT("/dev/poll init failed\n");
			goto error;
		}
		#endif

		#ifdef HAVE_SELECT
		if ((poll_method == POLL_SELECT) && (select_init(h) < 0))
		{
			LM_CRIT("select init failed\n");
			goto error;
		}
		#endif


		if ((poll_method == POLL_POLL) && (poll_init(h) < 0))
		{
			LM_CRIT("poll init failed\n");
			goto error;
		}

		break;

	#ifdef HAVE_EPOLL
	case POLL_EPOLL:
		if (epoll_init(h) < 0)
		{
			LM_CRIT("epoll init failed\n");
			goto error;
		}
		break;
	#endif

	#ifdef HAVE_KQUEUE
	case POLL_KQUEUE:
		h->kq_array = local_malloc(sizeof (*(h->kq_array)) * h->max_fd_no);
		if (h->kq_array == 0)
		{
			LM_CRIT("could not alloc kqueue event array\n");
			goto error;
		}

		memset((void*) h->kq_array, 0, sizeof (*(h->kq_array)) * h->max_fd_no);

		if (kqueue_init(h) < 0)
		{
			LM_CRIT("kqueue init failed\n");
			goto error;
		}
		
		break;
	#endif

	default:
		LM_CRIT("unknown/unsupported poll method %s (%d)\n",
				poll_method_str[poll_method], poll_method);
		goto error;
	}

	return 0;
error:
	return -1;
}

/*!
 * \brief destroys everything init_io_wait allocated
 * \param h IO handle
 */
void destroy_io_wait(io_wait_h* h)
{

	switch (h->poll_method)
	{

	#ifdef HAVE_EPOLL
	case POLL_EPOLL:
		epoll_destroy(h);
		if (h->ep_array)
		{
			local_free(h->ep_array);
			h->ep_array = 0;
		}
		break;
	#endif

	#ifdef HAVE_KQUEUE
	case POLL_KQUEUE:
		kqueue_destroy(h);
		if (h->kq_array)
		{
			local_free(h->kq_array);
			h->kq_array = 0;
		}
		
		break;
	#endif

	case POLL_POLL:
		poll_destroy(h);
		break;

	#ifdef HAVE_DEVPOLL
	case POLL_DEVPOLL:
		destroy_devpoll(h);
		break;
	#endif
	default: /*do  nothing*/
		;
	}

	if (h->fd_array)
	{
		local_free(h->fd_array);
		h->fd_array = 0;
	}


	if (h->fd_hash)
	{
		local_free(h->fd_hash);
		h->fd_hash = 0;
	}

}

