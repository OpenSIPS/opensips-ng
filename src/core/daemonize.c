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
 *  2010-02-xx  imported (bogdan)
 */

/*!
 * \file
 * \brief Setup the OpenSIPS daemon prozess
 */


#include <sys/types.h>

#define _XOPEN_SOURCE   /* needed on linux for the  getpgid prototype, but
                           openbsd 3.2 won't include common types (uint a.s.o)
                           if defined before including sys/types.h */
#define _XOPEN_SOURCE_EXTENDED /* same as above */
#define __USE_XOPEN_EXTENDED /* same as above, overrides features.h */
#define __EXTENSIONS__ /* needed on solaris: if XOPEN_SOURCE is defined
                          struct timeval defintion from <sys/time.h> won't
                          be included => workarround define _EXTENSIONS_ */
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h> /* setrlimit */
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#ifdef __OS_linux
#include <sys/prctl.h>
#endif

#include "daemonize.h"
#include "log.h"


/*!
 * \brief daemon init
 * \param name daemon name
 * \return return 0 on success, -1 on error
 */
int daemonize(char* name, int uid, int gid, char *pid_file, char *pgid_file,
									 char *chroot_dir, char *working_dir)
{
	FILE *pid_stream;
	pid_t pid;
	int r, p;
	int pid_items;
	int own_pgid;

	p=-1;
	own_pgid = 0;

	/* flush std file descriptors to avoid flushes after fork
	 *  (same message appearing multiple times)
	 *  and switch to unbuffered
	 */
	setbuf(stdout, 0);
	setbuf(stderr, 0);
	if (chroot_dir&&(chroot(chroot_dir)<0)){
		LM_CRIT("Cannot chroot to %s: %s\n", chroot_dir, strerror(errno));
		goto error;
	}
	
	if (chdir(working_dir)<0){
		LM_CRIT("Cannot chdir to %s: %s\n", working_dir, strerror(errno));
		goto error;
	}

	/* fork to become!= group leader*/
	if ((pid=fork())<0){
		LM_CRIT("Cannot fork:%s\n", strerror(errno));
		goto error;
	}else if (pid!=0){
		/* parent process => exit*/
		exit(0);
	}
	/* become session leader to drop the ctrl. terminal */
	if (setsid()<0){
		LM_WARN("setsid failed: %s\n",strerror(errno));
	} else {
		own_pgid = 1;
	}
	/* fork again to drop group  leadership */
	if ((pid=fork())<0){
		LM_CRIT("Cannot  fork:%s\n", strerror(errno));
		goto error;
	}else if (pid!=0){
		/*parent process => exit */
		exit(0);
	}

	/* added by noh: create a pid file for the main process */
	if (pid_file!=0){
		
		if ((pid_stream=fopen(pid_file, "r"))!=NULL){
			pid_items=fscanf(pid_stream, "%d", &p);
			fclose(pid_stream);
			if (p==-1 || pid_items <= 0){
				LM_WARN("pid file %s exists, but doesn't contain a valid"
					" pid number, replacing...\n", pid_file);
			} else 
			if (kill((pid_t)p, 0)==0 || errno==EPERM){
				LM_CRIT("running process found in the pid file %s\n",
					pid_file);
				goto error;
			}else{
				LM_WARN("pid file contains old pid, replacing pid\n");
			}
		}
		pid=getpid();
		if ((pid_stream=fopen(pid_file, "w"))==NULL){
			LM_ERR("unable to create pid file %s: %s\n", 
				pid_file, strerror(errno));
			goto error;
		}else{
			r = fprintf(pid_stream, "%i\n", (int)pid);
			if (r<=0)  {
				LM_ERR("unable to write pid to file %s: %s\n", 
					pid_file, strerror(errno));
				goto error;
			}
			fclose(pid_stream);
			/* change the owner and group also */
			if (chown( pid_file , uid?uid:-1, gid?gid:-1)!=0) {
				LM_ERR("failed to change owner of pid file %s: %s(%d)\n",
					pid_file, strerror(errno), errno);
				goto error;
			}
		}
	}

	if (pgid_file!=0){
		if ((pid_stream=fopen(pgid_file, "r"))!=NULL){
			pid_items=fscanf(pid_stream, "%d", &p);
			fclose(pid_stream);
			if (p==-1 || pid_items <= 0){
				LM_WARN("pgid file %s exists, but doesn't contain a valid"
					" pgid number, replacing...\n", pgid_file);
			}
		}
		if (own_pgid){
			pid=getpgid(0);
			if ((pid_stream=fopen(pgid_file, "w"))==NULL){
				LM_ERR("unable to create pgid file %s: %s\n",
					pgid_file, strerror(errno));
				goto error;
			}else{
				r = fprintf(pid_stream, "%i\n", (int)pid);
				if (r<=0)  {
					LM_ERR("unable to write pgid to file %s: %s\n", 
						pid_file, strerror(errno));
					goto error;
				}
				fclose(pid_stream);
				/* change the owner and group also */
				if (chown( pgid_file , uid?uid:-1, gid?gid:-1)!=0) {
					LM_ERR("failed to change owner of pid file %s: %s(%d)\n",
						pgid_file, strerror(errno), errno);
					goto error;
				}
			}
		}else{
			LM_WARN("we don't have our own process so we won't save"
					" our pgid\n");
			unlink(pgid_file); /* just to be sure nobody will miss-use the old
								  value*/
		}
	}

	/* try to replace stdin, stdout & stderr with /dev/null */
	if (freopen("/dev/null", "r", stdin)==0){
		LM_WARN("unable to replace stdin with /dev/null: %s\n",
			strerror(errno));
		/* continue, leave it open */
	};
	if (freopen("/dev/null", "w", stdout)==0){
		LM_WARN("unable to replace stdout with /dev/null: %s\n",
			strerror(errno));
		/* continue, leave it open */
	};
	/* close stderr only if not to be used */
	if ( log_syslog && (freopen("/dev/null", "w", stderr)==0)){
		LM_WARN("unable to replace stderr with /dev/null: %s\n",
			strerror(errno));
		/* continue, leave it open */
	};

	/* close any open file descriptors */
	closelog();

	/* 32 is the maximum number of inherited open file descriptors */
	for (r=3; r < 32; r++){
			close(r);
	}

	if (log_syslog)
		openlog(name, LOG_PID|LOG_CONS, log_facility);

	return  0;

error:
	return -1;
}


/* converts a username into uid:gid,
 * returns -1 on error & 0 on success */
int user2uid(int* uid, int* gid, char* user)
{
	char* tmp;
	struct passwd *pw_entry;
	
	if (user){
		*uid=strtol(user, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			pw_entry=getpwnam(user);
			if (pw_entry==0){
				goto error;
			}
			*uid=pw_entry->pw_uid;
			if (gid) *gid=pw_entry->pw_gid;
		}
		return 0;
	}
error:
	return -1;
}


int group2gid(int* gid, char* group)
{
	char* tmp;
	struct group  *gr_entry;
	
	if (group){
		*gid=strtol(group, &tmp, 10);
		if ((tmp==0) ||(*tmp)){
			/* maybe it's a string */
			gr_entry=getgrnam(group);
			if (gr_entry==0){
				goto error;
			}
			*gid=gr_entry->gr_gid;
		}
		return 0;
	}
error:
	return -1;
}



/*!
 * \brief set daemon user and group id
 * \param uid user id
 * \param gid group id
 * \return return 0 on success, -1 on error
 */
int do_suid( int uid, int gid)
{
	if (gid){
		if(setgid(gid)<0){
			LM_CRIT("cannot change gid to %d: %s\n", gid, strerror(errno));
			goto error;
		}
	}
	
	if(uid){
		if(setuid(uid)<0){
			LM_CRIT("cannot change uid to %d: %s\n", uid, strerror(errno));
			goto error;
		}
	}

	return 0;
error:
	return -1;
}



/*!
 * \brief try to increase the open file limit
 * \param target target that should be reached
 * \return return 0 on success, -1 on error
 */
int increase_open_fds(unsigned int target)
{
	struct rlimit lim, orig;
	
	if (getrlimit(RLIMIT_NOFILE, &lim)<0){
		LM_CRIT("cannot get the maximum number of file descriptors: %s\n",
				strerror(errno));
		goto error;
	}
	orig=lim;
	LM_DBG("current open file limits: %lu/%lu\n",
			(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
	if ((lim.rlim_cur==RLIM_INFINITY) || (target<=lim.rlim_cur))
		/* nothing to do */
		goto done;
	else if ((lim.rlim_max==RLIM_INFINITY) || (target<=lim.rlim_max)){
		lim.rlim_cur=target; /* increase soft limit to target */
	}else{
		/* more than the hard limit */
		LM_INFO("trying to increase the open file limit"
				" past the hard limit (%ld -> %d)\n", 
				(unsigned long)lim.rlim_max, target);
		lim.rlim_max=target;
		lim.rlim_cur=target;
	}
	LM_DBG("increasing open file limits to: %lu/%lu\n",
			(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max);
	if (setrlimit(RLIMIT_NOFILE, &lim)<0){
		LM_CRIT("cannot increase the open file limit to"
				" %lu/%lu: %s\n",
				(unsigned long)lim.rlim_cur, (unsigned long)lim.rlim_max,
				strerror(errno));
		if (orig.rlim_max>orig.rlim_cur){
			/* try to increase to previous maximum, better than not increasing
		 	* at all */
			lim.rlim_max=orig.rlim_max;
			lim.rlim_cur=orig.rlim_max;
			if (setrlimit(RLIMIT_NOFILE, &lim)==0){
				LM_CRIT("maximum number of file descriptors increased to"
					" %u\n",(unsigned)orig.rlim_max);
			}
		}
		goto error;
	}
done:
	return 0;
error:
	return -1;
}



/*!
 * \brief enable or disable core dumps
 * \param enable set to 1 to enable, to 0 to disable
 * \param size core dump size
 * \return return 0 on success, -1 on error
 */
int set_core_dump(int enable, unsigned int size)
{
	struct rlimit lim, newlim;
	
	if (enable){
		#ifdef __OS_linux
		/* setsid may disables core dumping on linux, reenable it */
		if (prctl(PR_SET_DUMPABLE, 1)) {
			LM_ERR("Cannot enable core dumping\n");
		}
		#endif
		if (getrlimit(RLIMIT_CORE, &lim)<0){
			LM_CRIT("cannot get the maximum core size: %s\n",
					strerror(errno));
			goto error;
		}
		if (lim.rlim_cur<size){
			/* first try max limits */
			newlim.rlim_max=RLIM_INFINITY;
			newlim.rlim_cur=newlim.rlim_max;
			if (setrlimit(RLIMIT_CORE, &newlim)==0) goto done;
			/* now try with size */
			if (lim.rlim_max<size){
				newlim.rlim_max=size;
			}
			newlim.rlim_cur=newlim.rlim_max;
			if (setrlimit(RLIMIT_CORE, &newlim)==0) goto done;
			/* if this failed too, try rlim_max, better than nothing */
			newlim.rlim_max=lim.rlim_max;
			newlim.rlim_cur=newlim.rlim_max;
			if (setrlimit(RLIMIT_CORE, &newlim)<0){
				LM_CRIT("could increase core limits at all: %s\n",
					strerror (errno));
			}else{
				LM_CRIT("core limits increased only to %lu\n",
					(unsigned long)lim.rlim_max);
			}
			goto error; /* it's an error we haven't got the size we wanted*/
		}
		goto done; /*nothing to do */
	}else{
		/* disable */
		newlim.rlim_cur=0;
		newlim.rlim_max=0;
		if (setrlimit(RLIMIT_CORE, &newlim)<0){
			LM_CRIT("failed to disable core dumps: %s\n",
				strerror(errno));
			goto error;
		}
	}
done:
	LM_DBG("core dump limits set to %lu\n", (unsigned long)newlim.rlim_cur);
	return 0;
error:
	return -1;
}
