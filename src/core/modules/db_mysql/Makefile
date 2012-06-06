# $Id: Makefile 5901 2009-07-21 07:45:05Z bogdan_iancu $
#
# DB_Mysql LIB makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
ROOT_PATH=../../../..

include $(ROOT_PATH)/Makefile.defs
NAME=db_mysql.so

ifeq ($(CROSS_COMPILE),)
HAS_MYSQLCFG=$(shell if which mysql_config >/dev/null 2>/dev/null;then echo YES; fi)
endif

ifeq ($(HAS_MYSQLCFG),YES)

	# use autodetection
	DEFS += $(shell mysql_config --include | sed 's/\(-I.*\)\/mysql/\1/g' )
	LIBS = $(shell mysql_config --libs_r)

else

	# use standard know paths
	# mysql.h locations (freebsd,openbsd  solaris)
	DEFS +=-I$(LOCALBASE)/include -I$(LOCALBASE)/include/mysql \
		-I$(LOCALBASE)/mysql/include/mysql -I$(LOCALBASE)/mysql/include \
		-I$(SYSBASE)/include/mysql

	# libmysqlclient locations on RH/Suse, Solaris /OpenBSD, FreeBSD
	# (Debian does the right thing and puts it in /usr/lib)
	LIBS=-L$(SYSBASE)/lib/mysql -L$(LOCALBASE)/lib -L$(LOCALBASE)/lib/mysql \
		-L$(LOCALBASE)/mysql/lib/mysql/ \
		-L$(LOCALBASE)/mysql/lib \
		-L$(SYSBASE)/lib64/mysql \
		-lm -lmysqlclient -lz

endif

include $(ROOT_PATH)/Makefile.libs
