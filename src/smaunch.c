/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/prctl.h>
#include <linux/securebits.h>

#include "smack-utils-fs.h"
#include "smaunch-smack.h"
#include "smaunch-fs.h"

const char *default_fs_key = 0;

int smaunch_init(const char *smackdb, const char *fsdb)
{
	int result;

	assert(smackdb);
	assert(fsdb);

	smack_fs_mount_point();
	result = smaunch_smack_load_database(smackdb);
	if (result < 0) /* todo: syntax error? */
		return result;

	result = smaunch_fs_load_database(fsdb);
	if (result < 0) /* todo: syntax error? */
		return result;

	return result;
}

void smaunch_context_start(const char *defskey)
{
	assert(defskey);
	assert(smaunch_smack_has_database());
	assert(smaunch_fs_has_database());

	smaunch_smack_context_start();
	smaunch_fs_context_start();
	default_fs_key = defskey;
}

int smaunch_context_add(const char *key)
{
	int rsm, rfs;

	assert(key);
	assert(smaunch_smack_has_database());
	assert(smaunch_fs_has_database());

	rsm = smaunch_smack_context_add(key);
	rfs = smaunch_fs_context_add(key);
	if (!rfs)
		default_fs_key = 0;

	if (!rsm)
		return rfs==-ENOENT ? 0 : rfs;

	if (!rfs)
		return rsm==-ENOENT ? 0 : rsm;

	if (rsm != -ENOENT)
		return rsm;

	return rfs;
}

int smaunch_context_apply()
{
	int result;

	assert(smaunch_smack_has_database());
	assert(smaunch_fs_has_database());

	if (default_fs_key) {
		result = smaunch_fs_context_add(default_fs_key);
		if (result)
			return result;
	}
		
	result = smaunch_smack_context_apply();
	if (result)
		return result;

	result = smaunch_fs_context_apply();
	if (result)
		return result;

	return 0;
}

int smaunch_drop_caps()
{
	int sts;

	sts = prctl(PR_SET_SECUREBITS,
		   SECBIT_KEEP_CAPS_LOCKED |
		   SECBIT_NO_SETUID_FIXUP |
		   SECBIT_NO_SETUID_FIXUP_LOCKED |
		   SECBIT_NOROOT |
		   SECBIT_NOROOT_LOCKED);

	return sts ? -errno : 0;
}

#ifdef TEST
#include <stdio.h>

int main(int argc, char** argv)
{
#if 0
	char buffer[4096], command[1024], key[1024];
	int n;
	while(fgets(buffer,sizeof buffer,stdin)) {
		n = sscanf(buffer,"%s %s",command,key);
		switch (n) {
		case 1:
			if (0 == strcmp(command, "apply") && smaunch_smack_has_database()) {
				printf("[apply] %d\n",smaunch_smack_context_apply());
				smaunch_smack_context_start();
				continue;
			}
			break;
		case 2:
			if (0 == strcmp(command, "load")) {
				n = smaunch_smack_load_database(key);
				printf("[load] %d\n",n);
				if (n == 0)
					smaunch_smack_dump_all(1);
				continue;
			}
			if (0 == strcmp(command, "add") && smaunch_smack_has_database()) {
				printf("[add] %d\n",smaunch_smack_context_add(key));
				continue;
			}
			break;
		}
	}
#else
	int sts;
	const char *substs[][2] = { { "%user", NULL } };

	substs[0][1] = getenv("USER");

	smaunch_fs_set_substitutions(substs, 1);

	sts = smaunch_init("/home/jb/dev/smaunch/src/db.smack", "/home/jb/dev/smaunch/src/db.fs");
	printf("init %d\n", sts);

	smaunch_context_start("restricted");
	printf("startdd\n");

	while(*++argv) {
		sts = smaunch_context_add(*argv);
		printf("add %s %d\n",*argv,sts);
	}

	sts = smaunch_context_apply();
	printf("apply %d\n", sts);

	system("/bin/sh");
#endif
	return 0;
}
#endif

