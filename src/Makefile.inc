
LIBSRC = \
	smaunch-fs.c \
	smaunch.c \
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

.PHONY: all clean clean-all

all: smaunch$X

clean-all: clean
	$(RM) libsmaunch$X.a smaunch$X

clean:
	$(RM) $(LIBOBJ) main-smaunch-$X.o 

smaunch$X: main-smaunch-$X.o libsmaunch$X.a
	$(CC) -o $@ $^
ifeq "$D" "n"
	strip $@
endif
ifeq "$S" "0"
	sudo /sbin/setcap cap_sys_admin,cap_mac_admin=+pe-i $@
endif

libsmaunch$X.a: $(LIBOBJ)
	$(AR) rcs $@ $^

%-$X.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# from "gcc -MM *.c | sed 's:\.o:-$X.o:'"

buffer-$X.o: buffer.c buffer.h
main-smaunch-$X.o: main-smaunch.c smaunch-fs.h smaunch.h
parse-$X.o: parse.c parse.h
smaunch-$X.o: smaunch.c smaunch-fs.h
smaunch-fs-$X.o: smaunch-fs.c smaunch-fs.h parse.h buffer.h

