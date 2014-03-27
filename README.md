smaunch
=======

Linux launcher library combining smack and namespace

Current state
-------------

This project is in a premature state.

It is a good proof of concept with solid base for futur
integration.

It still have to be fully documented.

What does it do?
----------------

It is based on the idea that a user have in ssh or prompt
all the authorisations that it can have and delegate
to the launcher the security isolation of the launched
processes based on their manifests.

Knowing key authorisations of an application,
the launcher removes smacks capacities and remount
parts of the file system according to rules.

This is done with 2 sub substems 'smaunch-smack' and
'smaunch-fs'. Both of them are tuned by database.

What is provided (currently)
----------------------------

When typing `make` in the subdirectory `src` it produces
the executable `smaunch` (and its variants) and the static 
library `libsmaunch.a` (and its variant).

The database exemples `db.smack` and `db.fs` are actually dummy
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

