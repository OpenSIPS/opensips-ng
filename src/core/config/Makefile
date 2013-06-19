#
# Configuration file parser makefile
#
# 
# WARNING: do not run this directly, it should be run by the master Makefile
ROOT_PATH=../../..

SRC_FILES=config.tab.c lex.yy.c

include $(ROOT_PATH)/Makefile.defs

all: $(SRC_FILES)

.NOTPARALLEL: lex.yy.c
lex.yy.c: config.lex config.tab.h
	@echo "Generating lexer"
	$(Q)$(LEX) $<

.NOTPARALLEL: config.tab.c config.tab.h
config.tab.c config.tab.h: config.y
	@echo "Generating parser"
	$(Q)$(YACC) $(YACC_FLAGS) $<
