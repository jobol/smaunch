.PHONY: all

all: smaunch

DEPSRC = \
	smack-utils-label.c \
	smack-utils-coda.c \
	smack-utils-access.c \
	smack-utils-fs.c \
	smaunch-smack.c \
	smaunch-fs.c \
	parse.c \
	buffer.c

DEPINC = \
	smack-utils-label.h \
	smack-utils-coda.h \
	smack-utils-access.h \
	smack-utils-fs.h \
	smaunch-smack.h \
	smaunch-fs.h \
	parse.h \
	buffer.h

smaunch: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -Wall -g -o smaunch smaunch.c $(DEPSRC)


