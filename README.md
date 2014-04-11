smaunch
=======

Linux launcher library combining smack, namespace
and key permission manager KEYZEN (since 10 Apr. 2014).

Current state
-------------

This project is in a quasi-mature state.

It is a good proof of concept with solid base for futur
integration.

What does it do?
----------------

It is based on the idea that a user have in ssh or prompt
all the authorisations that it can have and delegate
to the launcher the security isolation of the launched
processes based on their manifests.

Knowing key authorisations of an application the launcher:
 - set the authorisation keys to keyzen key permission system;
 - removes unwanted smack's capacities;
 - remount parts of the file system for hiding and making read-only.

What is provided (currently)
----------------------------

When typing `make` in the subdirectory `src` it produces
the executables `smaunch`, `smaunch-launcher`, `smaunch-attr`
and the static library `libsmaunch.a` .

The database exemples `db.smack` and `db.fs` are actually dummy
but are providing good entries to check and experiment.

Installation
------------

First, you need to install keyzen, to compile it (for getting libkeyzen.a).
See https://github.com/jobol/keyzen

```
# install and compile
git clone https://github.com/jobol/smaunch
cd smaunch/src
make

# compile and install the binary databases
sudo ./smaunch -cf db.fs /etc/.smaunch.fs.bin
sudo ./smaunch -cs db.smack /etc/.smaunch.smack.bin
```

Note that the `makefile.inc` expects keyzen files to be in ../../keyzen/src.
Change it if needed line 25:
```
KEYZENDIR = ../../keyzen/src
```

The program `smaunch-launcher`
------------------------------

This is the secure launcher of applications.

This program isn't intended to be run directly.
Here is how it is used.

Suppose that you want to run the webapp W using
the webruntime X. The normal way is to define a
symbolic link L to X and let X retrieve the
webapp W from the link name and location of L.

With `smaunch-launcher`, the symbolic link L is set
to point to `smaunch-launcher` and the extended security
attribute `security.smaunch-launcher` of L will tell
to `smaunch-launcher` the path of X, the permission keys
to activate and the substitutions to add when remounting
the filesystem parts.

Then running W will:
 - launch `smaunch-launcher`
 - `smaunch-launcher` will set the perssion keys to keyzen
 - `smaunch-launcher` will alter the smack rules according with the authorized keys
 - `smaunch-launcher` will alter the filesystem according to the authorized keys
 - `smaunch-launcher` will launch X within the new security context as if it was launched by the link
 - X will process the name and location of L to know what application to run

And what for native applications? Suppose that you have a native application N.
Then you make in place of N a link L to `smaunch-launcher` and set the
application to launch to N (and also permission keys to set and substitutions).

The program `smaunch-attr` is a facility to set the extended security
attributes `security.smaunch-launcher`.

### example

Suppose that you want to launch /usr/bin/bash with only the permission keys 'user' 
'WRT' (that are available in the database). 
Suppose you compiled and installed keyzen and smaunch.
Suppose you are running keyzen-fs, making keyzen permission key manager active.

```
# create the link
ln -s smaunch-launcher mybash

# set its attr
smaunch-attr -s "@/usr/bin/bash
=user
=WRT
" mybash

# run now
./mybash
```

The program `smaunch-attr`
--------------------------

It is used to set or read the data used by smaunch-launcher.

Usage: `smaunch-attr [-s value] files...

See below for documentation on the format of the keys.


The program `smaunch`
---------------------

This executable demonstrate the use of `libsmaunch`.

It allows the following operations:
- check the databases
- compile the databases
- launch a program within security restriction.

Type `smaunch --help' to get usage.

Values of attribute 'security.smaunch-launcher'
-----------------------------------------------

The value is made of lines terminated by line feed (just LF, not CRLF please).

The first character of the line tells the kind of the entry:
 - @ path of the executable target
 - % substitution for the filesystem
 - = permit key
 - - deny key
 - ! blanket-prompt key
 - + session-prompt key
 - * one-shot-prompt key

For the keys and the path, the value is just following the kind of entry
until the terminating character.

For substitutions, an equal sign '=' separate the key from its value like
`%key=value`. The substitution key `%key` of the filesystem database
will then be replaced with `value`. It is valid is none of `key` and `value` 
either is empty or contain slash '/'.

Not currently checked but it will be invalid to repeat a key or to override
predfined substitutions.

Database Format
---------------

Example for smack:
```
-- amb access
amb.read
	AMB				r
	AMB::list		r
amb.write
	AMB				rw
	AMB::list		rw
```

Example for fs:
```
-- user access
user
	-		/home				-- dont see other users
	+rw		/home/%user			-- see itself
	+r		/sys/fs/smackfs		-- disable write access to change-rule

-- basic restricted access
restricted
	-		/home								-- dont see any other user
	+rw		/home/%user/.config/%appid			-- access to config
	+r		/home/%user/share					-- shared data
	+rw		/home/%user/share/%appid			-- own shared data
	+rw		/home/%user/share/.cert/%cert		-- same certificate
	+r		/sys/fs/smackfs						-- disable change-rule
```


The 2 databases, for smack and fs, are following the
same format:

- fields are separated by tabs or spaces;
- spaces or tabs at begin are detected and counting;
- at the beginning of a field 2 dashs are beginning a comment.

keys aren't preceded by space or tab

For security, smackfs should be remounted read only.


