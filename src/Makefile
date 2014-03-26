.PHONY: all

all: smaunch smaunchi smaunchg smaunchig

DEPSRC = \
	smack-utils-label.c \
	smack-utils-coda.c \
	smack-utils-fs.c \
	smaunch-smack.c \
	smaunch-fs.c \
	smaunch.c \
	parse.c \
	buffer.c

DEPINC = \
	smack-utils-label.h \
	smack-utils-coda.h \
	smack-utils-fs.h \
	smaunch-smack.h \
	smaunch-fs.h \
	smaunch.h \
	parse.h \
	buffer.h

# optimized version
smaunch: main-smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=0 -Wall -O3 -DNDEBUG -o $@ main-smaunch.c $(DEPSRC)
	strip $@
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin=+pe-i $@

# debug version
smaunchg: main-smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=0 -Wall -g -o $@ main-smaunch.c $(DEPSRC)
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin=+pe-i $@

# optimized simulation version
smaunchi: main-smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=1 -Wall -O3 -DNDEBUG -o $@ main-smaunch.c $(DEPSRC)
	strip $@

# debug simulation version
smaunchig: main-smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=1 -Wall -g -o $@ main-smaunch.c $(DEPSRC)



