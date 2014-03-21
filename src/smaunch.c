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

#if !defined(SIMULATION)
#define SIMULATION 0
#endif

static const char *default_fs_key = 0;

int smaunch_init(const char *smackdb, const char *fsdb, const char *defskey)
{
	int result;

	assert(smackdb);
	assert(fsdb);

	default_fs_key = 0;

#if !SIMULATION
	if (!smack_fs_mount_point())
		return -EACCES;
#endif

	result = smaunch_smack_load_database(smackdb);
	if (result < 0) /* todo: syntax error? */
		return result;

	result = smaunch_fs_load_database(fsdb);
	if (result < 0) /* todo: syntax error? */
		return result;

	if (!smaunch_fs_has_key(defskey))
		return -ENOENT;

	default_fs_key = defskey;
	return 0;
}

int smaunch_is_ready()
{
	return !!default_fs_key;
}

int smaunch_apply(char **keys)
{
	int result, setdef, rsm, rfs;

	assert(smaunch_is_ready());

	smaunch_smack_context_start();
	smaunch_fs_context_start();

	setdef = 1;
	while (*keys) {

		rsm = smaunch_smack_context_add(*keys);
		if (rsm && rsm != -ENOENT)
			return rsm;

		rfs = smaunch_fs_context_add(*keys);
		if (!rfs)
			setdef = 0;
		else if (rfs != -ENOENT || rsm)
			return rfs;

		keys++;
	}

	if (setdef) {
		result = smaunch_fs_context_add(default_fs_key);
		assert(!result);
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

int smaunch_exec(char **keys, const char *filename, char **argv, char **envp);
int smaunch_fork_exec(char **keys, const char *filename, char **argv, char **envp);


#ifdef TEST
#include <stdio.h>

int main(int argc, char** argv)
{
	int sts;
	const char *substs[][2] = {
		{ "%user", NULL },
		{ "%uid",  NULL }
	};

	substs[0][1] = getenv("USER");
	substs[1][1] = getenv("UID");

	smaunch_fs_set_substitutions(substs, 2);

	sts = smaunch_init("db.smack", "db.fs", "restricted");
	printf("init %d\n", sts);

	sts = smaunch_apply(++argv);
	printf("apply %d\n", sts);

	system("/bin/sh");
	return 0;
}
#endif

