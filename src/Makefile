.PHONY: all

all: smaunch smaunchi smaunchg smaunchig

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

# optimized version
smaunch: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=0 -O3 -DNDEBUG -o $@ smaunch.c $(DEPSRC)
	strip $@
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin=+pe-i $@

# debug version
smaunchg: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=0 -Wall -g -o $@ smaunch.c $(DEPSRC)
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin=+pe-i $@

# optimized simulation version
smaunchi: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=1 -O3 -DNDEBUG -o $@ smaunch.c $(DEPSRC)
	strip $@

# debug simulation version
smaunchig: smaunch.c $(DEPSRC) $(DEPINC)
	gcc -DTEST -DSIMULATION=1 -Wall -g -o $@ smaunch.c $(DEPSRC)



