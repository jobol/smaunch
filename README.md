smaunch
=======

Linux launcher library combining smack and namespace

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

The program `smaunch`
---------------------

This executable demonstrate the use of `libsmaunch`.

It allows the following operations:
- check the databases
- compile the databases
- launch a program within security restriction.

Type `smaunch --help' to get usage or/and RTFC.

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

