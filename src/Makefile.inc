
LIBSRC = \
	smack-utils-label.c \
	smack-utils-coda.c \
	smack-utils-fs.c \
	smaunch-smack.c \
	smaunch-fs.c \
	smaunch.c \
	launch-spec.c \
	parse.c \
	buffer.c

LIBOBJ = $(LIBSRC:.c=-$X.o)

CFLAGS = -Wall -Werror -fPIC -DSIMULATION=$S

ifeq "$D" "n"
CFLAGS += -O3 -DNDEBUG
LDFLAGS += -s
else
CFLAGS += -g
endif
LDLIBS += -L. -lsmaunch$X

KEYZENDIR = ../../keyzen/src
CFLAGS += -I$(KEYZENDIR)
LDLIBS += -L$(KEYZENDIR) -lkeyzen

.PHONY: all clean clean-all

TARGETS = libsmaunch$X.a smaunch$X smaunch$X-launcher smaunch$X-attr

all: $(TARGETS)

clean-all: clean
	$(RM) *-$X.o $(TARGETS)

clean:
	$(RM) *-$X.o

smaunch$X: main-smaunch-$X.o libsmaunch$X.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
ifeq "$S" "0"
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin=+pe-i $@
endif

smaunch$X-launcher: smaunch-launcher-$X.o libsmaunch$X.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
ifeq "$S" "0"
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin,cap_fowner=+pe-i $@
	sudo /usr/bin/setfattr -n security.keyzen -v keyzen.admin $@
endif

smaunch$X-attr: smaunch-attr-$X.o libsmaunch$X.a
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
ifeq "$S" "0"
	sudo /sbin/setcap cap_sys_admin,cap_fowner=+pe-i $@
endif

libsmaunch$X.a: $(LIBOBJ)
	$(AR) rcs $@ $^

%-$X.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# from "gcc -MM *.c | sed 's:\.o:-$X.o:'"

buffer-$X.o: buffer.c buffer.h
launch-spec-$X.o: launch-spec.c
main-smaunch-$X.o: main-smaunch.c smaunch-smack.h smaunch-fs.h smaunch.h \
 parse.h
parse-$X.o: parse.c parse.h
smack-utils-coda-$X.o: smack-utils-coda.c smack-utils-coda.h
smack-utils-fs-$X.o: smack-utils-fs.c smack-utils-fs.h
smack-utils-label-$X.o: smack-utils-label.c smack-utils-label.h
smaunch-$X.o: smaunch.c smack-utils-fs.h smaunch-smack.h smaunch-fs.h
smaunch-attr-$X.o: smaunch-attr.c launch-spec.h
smaunch-fs-$X.o: smaunch-fs.c smaunch-fs.h parse.h buffer.h
smaunch-launcher-$X.o: smaunch-launcher.c parse.h smaunch-smack.h \
 smaunch-fs.h smaunch.h $(KEYZENDIR)/keyzen.h \
 $(KEYZENDIR)/keyzen-constants.h launch-spec.h
smaunch-smack-$X.o: smaunch-smack.c smaunch-smack.h smack-utils-label.h \
 smack-utils-coda.h smack-utils-fs.h buffer.h parse.h

