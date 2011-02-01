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



#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "log.h"
#include "init.h"
#include "version.h"
#include "globals.h"
#include "daemonize.h"
#include "threading.h"
#include "timer.h"
#include "config.h"
#include "modules.h"
#include "mem/mem.h"
#include "config/params.h"
#include "net/proto.h"
#include "net/net_params.h"
#include "reactor/reactor.h"
#include "parser/msg_parser.h"
#include "parser/parse_content.h"
#include "resolve/resolve.h"
#include "db/db_to_user.h"
#include "msg_handler.h"
#include "mi/mi_core.h"


/*************************  INTERNAL VARIABLES ****************************/

/* opensips version string */
static char* osips_version = OPENSIPS_FULL_VERSION;

/* opensips flags string */
static char* osips_flags=OPENSIPS_COMPILE_FLAGS;

/* opensips built date */
char osips_built_date[]= __TIME__ " " __DATE__ ;

/* size of shared memory in Mb */
static unsigned long shmem_size = SHM_MEM_SIZE;

/* name of the core config file */
static char *cfg_file = CFG_FILE;


/**************************  GLOBAL VARIABLES *****************************/

reactor_t *reactor_in;
reactor_t *reactor_out;

time_t startup_time;

int osips_argc;

char **osips_argv;

int tcp_disable;
int tls_disable;
int sctp_disable;

/***************************  CFG VARIABLES ******************************/
/* become daemon */
static int become_daemon = 1;

/* number of worker children */
static int children = 4;

/* working directory */
static char *working_dir = NULL;

/* root directory */
static char *chroot_dir = NULL;

/* system user to run as */
static char *sys_user = NULL;

/* system group to run as */
static char *sys_group = NULL;

/* should be core dump generated ? */
static int do_coredump = 1;

/* new limit for open files ? */
static int open_files_limit = -1;

/* PID file */
static char *pid_file = NULL;

/* PGID file */
static char *pgid_file = NULL;


static config_param_t core_params[] = {
	{"daemon",       &become_daemon,    PARAM_TYPE_INT,        0},
	{"children",     &children,         PARAM_TYPE_INT,        0},
	{"working_dir",  &working_dir,      PARAM_TYPE_STRING,     0},
	{"chroot_dir",   &chroot_dir,       PARAM_TYPE_STRING,     0},
	{"user",         &sys_user,         PARAM_TYPE_STRING,     0},
	{"group",        &sys_group,        PARAM_TYPE_STRING,     0},
	{"do_coredump",  &do_coredump,      PARAM_TYPE_INT,        0},
	{"rt_coredump",  &rt_coredump,      PARAM_TYPE_INT,        0},
	{"open_files",   &open_files_limit, PARAM_TYPE_INT,        0},
	{"pid_file",     &pid_file,         PARAM_TYPE_STRING,     0},
	{"pgid_file",    &pgid_file,        PARAM_TYPE_STRING,     0},
	{0, 0, 0, 0}
};

static param_section_t core_section = {
	"core",
	core_params
};

static config_param_t log_params[] = {
	{"debug",        &debug,              PARAM_TYPE_INT,     0},
	{"memlog",       &memlog,             PARAM_TYPE_INT,     0},
	{"memdump",      &memdump,            PARAM_TYPE_INT,     0},
	{"log_syslog",   &log_syslog,         PARAM_TYPE_INT,     0},
	{"log_name",     &log_name,           PARAM_TYPE_STRING,  0},
	{"log_facility", set_syslog_facility, PARAM_TYPE_STRING|PARAM_TYPE_FUNC,0},
	{0, 0, 0, 0}
};

static param_section_t log_section = {
	"log",
	log_params
};

static config_param_t net_params[] = {
	{"listen",         register_listener, PARAM_TYPE_STRING|PARAM_TYPE_FUNC,0},
	{"dns_try_ipv6",   &dns_try_ipv6,     PARAM_TYPE_INT, 0},
	{"net_tos",        set_net_tos,       PARAM_TYPE_STRING|PARAM_TYPE_FUNC,0},
	#ifdef USE_MCAST
	{"mcast_ttl",      &mcast_ttl,        PARAM_TYPE_INT, 0},
	{"mcast_loopback", &mcast_loopback,   PARAM_TYPE_INT, 0},
	#endif
	{"udp_maxbuffer",  &udp_maxbuffer,    PARAM_TYPE_INT, 0},
	{"udp_min_size",   &udp_min_size,     PARAM_TYPE_INT, 0},
	{"tcp_max_size",   &tcp_max_size,     PARAM_TYPE_INT, 0},
	{"tcp_via_alias",  &tcp_via_alias,    PARAM_TYPE_INT, 0},
	{"tcp_lifetime",   &tcp_lifetime,     PARAM_TYPE_INT, 0},
	{"tcp_write_timeout",     &tcp_write_timeout,      PARAM_TYPE_INT, 0},
	{"tcp_vconnect_timeout",  &tcp_connect_timeout,    PARAM_TYPE_INT, 0},
	{"tcp_max_connections",   &tcp_max_connections,    PARAM_TYPE_INT, 0},
	{0, 0, 0, 0}
};

static param_section_t net_section = {
	"net",
	net_params
};

static config_param_t modules_params[] = {
	{"path",   set_load_module_path, PARAM_TYPE_STRING|PARAM_TYPE_FUNC,0},
	{"load",   load_core_module,     PARAM_TYPE_STRING|PARAM_TYPE_FUNC,0},
	{0, 0, 0, 0}
};

static param_section_t modules_section = {
	"modules",
	modules_params
};



/************************** EXTERN VARIABLES *****************************/
extern FILE* yyin;
extern int yyparse();
extern int cfg_errors;


/**
 * Worker routine - implements an opensips thread
 */

static void* worker_thread(void *dispatcher)
{
	heap_node_t task;

	LM_DBG("starting thread\n");

	while (1)
	{
		/* get a task - this call will block until
		 * a task is available from dispatcher */
		get_task((dispatcher_t*) dispatcher, &task);

		/* run the task */
		if (task.flags & CALLBACK_COMPLEX_F)
		{
			fd_callback_complex * f = (fd_callback_complex *) task.cb;
			f(task.last_reactor, task.fd, task.cb_param);
		} else
		{
			task.cb(task.cb_param);
		}
	}
	return NULL;
}


static void do_shutdown(void)
{
	/* all threads are stoped at this point -> destroy everything */

	/* destroy the reactors & dispatchers */
	if (reactor_in) {
		if (reactor_in->disp)
			destroy_dispatcher(reactor_in->disp);
		destroy_reactor(reactor_in);
	}
	if (reactor_out)
		destroy_reactor(reactor_out);

	/* destroy timer */
	destroy_timer();

	/* destroy all listeners */
	destroy_all_listeners();

	destroy_mi_cmds();

	db_core_destroy();

	destroy_protos();
	destroy_all_core_module();
	shm_status();
	/* done */
}


/**
 * Main routine, start of the program execution.
 * \param argc the number of arguments
 * \param argv pointer to the arguments array
 * \return don't return on sucess, -1 on error
 * \see main_loop
 */
int main(int argc, char** argv)
{
	int gid=0, uid=0;
	char *options;
	char *tmp;
	int cfg_log_syslog = log_syslog;
	int c;
	FILE *cfg_stream;
	dispatcher_t * dispatcher;

	/***************** CLI OPTIONS ********************/

	/* process command line parameters */
	options="f:m:dFDVhw:t:u:g:P:G:";

	while((c=getopt(argc,argv,options))!=-1){
		switch(c){
			case 'f':
					/* name of the config file */
					cfg_file = optarg;
					break;
			case 'm':
					/* share memory size in Mb */
					shmem_size =strtol(optarg, &tmp, 10);
					if (tmp &&(*tmp)){
						LM_ERR("bad number for shared memory: -m %s\n",
							optarg);
						goto error0;
					}
					break;
			case 'd':
					/* increase debug level */
					debug ++;
					break;
			case 'F':
					/* stay in foreground */
					become_daemon = 0;
					break;
			case 'S':
					/* log to syslog, insted of stderr */
					cfg_log_syslog = 1;
					break;
			case 'V':
					printf("version: %s\n", osips_version);
					printf("flags: %s\n", osips_flags );
					printf("%s compiled on %s with %s\n", __FILE__,
							osips_built_date, COMPILER );
					exit(0);
					break;
			case 'h':
					printf("version: %s\n", osips_version);
					//printf("%s",help_msg);
					exit(0);
					break;
			case 'w':
					working_dir = optarg;
					break;
			case 't':
					chroot_dir = optarg;
					break;
			case 'u':
					sys_user = optarg;
					break;
			case 'g':
					sys_group = optarg;
					break;
			case 'P':
					pid_file = optarg;
					break;
			case 'G':
					pgid_file = optarg;
					break;
			case '?':
					if (isprint(optopt))
						LM_ERR("Unknown option `-%c`.\n", optopt);
					else
						LM_ERR("Unknown option character `\\x%x`.\n", optopt);
					goto error0;
			case ':':
					LM_ERR("Option `-%c` requires an argument.\n", optopt);
					goto error0;
			default:
					abort();
		}
	}


	/***************** FIRST STAGE INIT ********************/
	log_syslog = cfg_log_syslog;

	osips_argc = argc;
	osips_argv = argv;

	/* init shared memory */
	if ( init_sh_memory(shmem_size*1024*1024)!=0 ) {
		LM_ERR("failed to init shared memory");
		goto error0;
	}

	init_random();


	/***************** LOAD CONFIG FILE ********************/
	global_append_section( &core_section );
	global_append_section(  &log_section );
	global_append_section(  &net_section );
	global_append_section(  &modules_section );

	/* open core config file */
	cfg_stream = fopen (cfg_file, "r");
	if (cfg_stream==0){
		LM_ERR("cannot open config file(%s): %s\n", cfg_file,
				strerror(errno));
		goto error0;
	}

	/* load core config file */
	yyin = cfg_stream;
	if ((yyparse()!=0)||(cfg_errors)){
		LM_ERR("bad config file (%d errors)\n", cfg_errors);
		goto error0;
	}

	/*after cfg is loaded, we do not need this anymore */
	global_destroy_sections();


	/***************** SECOND STAGE INIT ********************/
	if (working_dir==NULL)
		working_dir = "/";

	/* conver user/group names to user/group ids */
	if (sys_user){
		if (user2uid(&uid, &gid, sys_user)<0){
			LM_ERR("bad user name/uid number: -u %s\n", sys_user);
			goto error0;
		}
	}
	if (sys_group){
		if (group2gid(&gid, sys_group)<0){
			LM_ERR("bad group name/gid number: -u %s\n", sys_group);
			goto error0;
		}
	}

	/* switch to daemon mode */
	if (become_daemon) {
		if ( daemonize( (log_name==0)?argv[0]:log_name, uid, gid,
		pid_file, pgid_file, chroot_dir, working_dir ) <0 )
			goto error0;
	}

	/* set signal mask */
	if (pt_init_signals()<0 ) {
		LM_ERR("failed to init signals\n");
		goto error0;
	}

	/* set the new user/group */
	if (do_suid( uid, gid )!=0) {
		LM_ERR("failed to switch user user %s and group %s\n",
			sys_user?sys_user:"n/a", sys_group?sys_group:"n/a" );
		goto error0;
	}

	/* set the coredump size */
	if (do_coredump) {
		set_core_dump(1, (shmem_size+4)*1024*1024);
	} else {
		set_core_dump(0, 0);
	}

	/* set the limit for open files (per application) */
	if (open_files_limit>0){
		if(increase_open_fds(open_files_limit)<0){ 
			LM_ERR("ERROR: error could not increase file limits\n");
			goto error0;
		}
	}

	/* print various OpenSIPS info at startup */
	LM_NOTICE("OpenSIPS version: %s\n", osips_version);
	LM_INFO("using %ld Mb shared memory\n", shmem_size);


	/*************** INIT NETWORK LISTNERS ******************/

	/* initialize the holders for known protocols */
	init_protos();

	/* if no registered socket, do auto-discover */
	if (registered_listeners==NULL) {
		if ( auto_register_listeners()<0 ) {
			LM_ERR("failed to auto discover / register listeners\n");
			goto error0;
		}
	}

	/* fix/prepare all registered listeners */
	if ( fix_all_listeners()<0 ) {
		LM_ERR("failed to fix network listerners\n");
		goto error0;
	}

	/* initialized listeners */
	if ( init_all_listeners()<0 ) {
		LM_ERR("failed to initialize network listerners\n");
		goto error0;
	}


	/***************** REACTOR INIT ********************/

	dispatcher = new_dispatcher();

	if( dispatcher == NULL ){
		LM_ERR("failed to create dispatcher\n");
		goto error0;
	}

	/* create reactor */
	reactor_in = new_reactor(REACTOR_IN, dispatcher);
	if (reactor_in == NULL) {
		LM_ERR("failed to create in reactor\n");
		goto error0;
	}

	reactor_out = new_reactor(REACTOR_OUT, dispatcher);
	if (reactor_out == NULL) {
		LM_ERR("failed to create out reactor\n");
		goto error0;
	}

	/* init DNS resolver */
	if( resolv_init() != 0){
		LM_ERR("failed to init DNS resolver\n");
		goto error0;
	}

	/* init DB part of core  */
	if( db_core_init() !=0 ){
		LM_ERR("failed to init database structures\n");
		goto error0;
	}



	/***************** START THREADS ********************/

	/* mark the starting of everything */
	time( &startup_time );

	/* load MI core commands */
	if ( init_mi_core()<0 ) {
		LM_ERR("failed to init MI core commands\n");
		goto error0;
	}

	/* init main thread - set name, ids, etc */
	if (init_main_thread("attendent")<0) {
		LM_ERR("failed to setup main thread\n");
		goto error0;
	}

	/* start workers */
	for (c = 0; c < children; c++)
		pt_create_thread("worker", worker_thread, dispatcher);

	/* Start the reactor in thread */
	if (reactor_start(reactor_in, "reactor in")<0) {
		LM_ERR("failed to start reactor\n");
		goto error0;
	}

	/* Start the reactor out thread */
	if (reactor_start(reactor_out, "reactor out")<0) {
		LM_ERR("failed to start reactor\n");
		goto error0;
	}

	/* start timer thread */
	if (start_timer_thread()<0) {
		LM_ERR("failed to start timer thread\n");
		goto error0;
	}

	if (db_start_threads(8)){
		LM_ERR("failed to start db threads\n");
		goto error0;
	}

	/* init core modules */
	if (init_all_core_module()<0) {
		LM_ERR("failed to init core modules\n");
		goto error0;
	}

	if( init_msg_handler() != 0){
		LM_ERR("failed to init msg handler\n");
		goto error0;
	}

	/***************** ACTIVATE EVERYTHING *****************/

	/* activate network listeners */
	for( c=PROTO_UDP ; c<PROTO_MAX ; c++ ) {
		struct socket_info *si;
		for (si=protos[c].listeners ; si ; si=si->next)
			submit_task(reactor_in,
				(fd_callback*)protos[c].funcs.event_handler,
				(void*)si, TASK_PRIO_READ_IO, si->socket, 0);
	}

	/* simply stay and wait for any signals */
	pt_wait_signals( do_shutdown );

	return 0;
error0:
	return -1;
}
