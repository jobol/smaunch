.PHONY: all

all: smaunch smaunchi

DEPSRC = \
	smack-utils-label.c \
	smack-utils-coda.c \
	smack-utils-fs.c \
	smaunch-smack.c \
	smaunch-fs.c \
	parse.c \
	buffer.c

DEPINC = \
	smack-utils-label.h \
	smack-utils-coda.h \
	smack-utils-fs.h \
	smaunch-smack.h \
	smaunch-fs.h \
	parse.h \
	buffer.h

smaunch: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=0 -Wall -g -o smaunch smaunch.c $(DEPSRC)

smaunchi: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=1 -Wall -g -o smaunchi smaunch.c $(DEPSRC)


