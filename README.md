smaunch
=======

Linux launcher library combining smack and namespace

Current state
-------------

Currently, it is not fully documented nor integrated.

It is a good proof of concept with solid base for futur
integration.

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
	+ro		/sys/fs/smackfs		-- disable write access to change-rule

-- basic restricted access
restricted
	-		/home								-- dont see any other user
	+rw		/home/%user/.config/%appid			-- access to config
	+r		/home/%user/share					-- shared data
	+rw		/home/%user/share/%appid			-- own shared data
	+rw		/home/%user/share/.cert/%cert		-- same certificate
	+ro		/sys/fs/smackfs						-- disable change-rule
```


The 2 databases, for smack and fs, are following the
same format:

- fields are separated by tabs or spaces;
- spaces or tabs at begin are detected and counting;
- at the beginning of a field 2 dashs are beginning a comment.

keys aren't preceded by space or tab
