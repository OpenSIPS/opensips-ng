#
# WARNING: requires gmake (GNU Make)
#  Arch supported: Linux, xxxxBSD, Solaris
#
#  History:
#  --------
#  2010-01-08  created (bogdan)
#


############################ Various control options ##########################
exclude_protos?= tls
exclude_modules= db_postgres db_mysql


############# Defines and exports - nothing to configure from here ###########

#include other Makefile resource files
makefile_defs=0
DEFS:=
include Makefile.defs

NAME=$(MAIN_NAME)
ALLDEP=Makefile Makefile.defs Makefile.rules
PROTO_DIR=src/core/net/
MODULE_DIR=src/core/modules/
SRC_DIRS=src/core src/core/mem/ src/core/locking src/core/config \
		 src/core/net src/core/dispatcher src/core/reactor src/core/builder \
		 src/core/parser src/core/parser/digest src/core/parser/contact \
		 src/core/parser/sdp src/core/mi src/core/resolve src/core/db 

# skip the SVN dir from protos list
override exclude_protos+= .svn 

# get names of available protos
protos=$(shell cd $(PROTO_DIR) ; list=`ls -1 `; for i in $$list; do if test -d $$i; then echo $$i; fi; done)

# skip the excluded protos
protos:=$(filter-out $(exclude_protos), $(protos) )

# build list of directories for protos
protos:=$(addprefix $(PROTO_DIR),$(protos))


# skip the SVN dir from modules list
override exclude_moduless+= .svn 

# get names of available modules
modules=$(shell cd $(MODULE_DIR) ; list=`ls -1 `; for i in $$list; do if test -d $$i; then echo $$i; fi; done)

# skip the excluded moduless
modules:=$(filter-out $(exclude_modules), $(modules) )

# build list of directories for modules
modules:=$(addprefix $(MODULE_DIR),$(modules))


#export relevant variables to the sub-makes
export DEFS PROFILE CC LD MKDEP MKTAGS CFLAGS LDFLAGS LIB_CFLAGS LIB_LDFLAGS 
export LIBS LEX YACC YACC_FLAGS
export PREFIX LOCALBASE SYSBASE
# export relevant variables for recursive calls of this makefile 
# (e.g. make deb)
export NAME RELEASE OS ARCH 



################### Rules - nothing to configure from here ####################

# include the common rules
include Makefile.rules

#extra targets 

.PHONY: all
all: $(NAME) protos modules

.PHONY: cfg_parser
cfg_parser:
	cd src/core/config; $(MAKE) cfg_parser

.PHONY: protos
protos:
	@set -e; \
	for r in $(protos) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -d "$$r" ]; then \
				echo  "" ; \
				echo  "" ; \
				$(MAKE) -C $$r ; \
			fi ; \
		fi ; \
	done 

.PHONY: modules
modules:
	@set -e; \
	for r in $(modules) "" ; do \
		if [ -n "$$r" ]; then \
			if [ -d "$$r" ]; then \
				echo  "" ; \
				echo  "" ; \
				$(MAKE) -C $$r ; \
			fi ; \
		fi ; \
	done 


.PHONY: dist
dist: tar

.PHONY: tar
tar: 
	$(TAR) -C .. \
		--exclude=$(notdir $(CURDIR))/tmp* \
		--exclude=$(notdir $(CURDIR))/debian* \
		--exclude=.svn* \
		--exclude=*.[do] \
		--exclude=*.so \
		--exclude=*.il \
		--exclude=$(notdir $(CURDIR))/$(NAME) \
		--exclude=*.gz \
		--exclude=*.bz2 \
		--exclude=*.tar \
		--exclude=*.patch \
		--exclude=.\#* \
		--exclude=*.swp \
		--exclude=*~ \
		${tar_extra_args} \
		-cf - $(notdir $(CURDIR)) | \
			(mkdir -p tmp/_tar1; mkdir -p tmp/_tar2 ; \
			    cd tmp/_tar1; $(TAR) -xf - ) && \
			    mv tmp/_tar1/$(notdir $(CURDIR)) \
			       tmp/_tar2/"$(NAME)-$(RELEASE)" && \
			    (cd tmp/_tar2 && $(TAR) \
			                    -zcf ../../"$(NAME)-$(RELEASE)_src".tar.gz \
			                               "$(NAME)-$(RELEASE)" ) ; \
			    rm -rf tmp/_tar1; rm -rf tmp/_tar2

