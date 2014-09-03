smaunch
=======

Linux launcher library setting filesystem namespace

Current state
-------------

It is a good proof of concept with solid base for futur
integration.

It still have to be fully documented.

What is provided (currently)
----------------------------

When typing `make` in the subdirectory `src` it produces
the executable `smaunch` (and its variants) and the static 
library `libsmaunch.a` (and its variant).

The database exemple `db.fs` is actually dummy
but are providing good entries to check and experiment.

The variants of the program and the library are providing
the following flavour: debug version (g), simulation version
optimized (i) and simulation version debug (ig).
The simulation versions are prompting their actions in
place of doing it.

The program `smaunch`
---------------------

This executable demonstrate the use of `libsmaunch`.

It allows the following operations:
- check the databases
- compile the databases
- launch a program within security restriction

Type `smaunch --help' to get usage or/and RTFC.

Database Format
---------------


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


The databases are following the format:

- fields are separated by tabs or spaces;
- spaces or tabs at begin are detected and counting;
- at the beginning of a field 2 dashs are beginning a comment.

keys aren't preceded by space or tab


