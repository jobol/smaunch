/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>

#include "smack-utils-fs.h"
#include "smaunch-smack.h"
#include "smaunch-fs.h"

#if !defined(SIMULATION)
#define SIMULATION 0
#endif

extern char **environ;

static const char *default_fs_key = 0;

int smaunch_init(const char *smackdb, const char *fsdb, const char *defskey)
{
	int result;

	assert(smackdb);
	assert(fsdb);

	default_fs_key = 0;

#if !SIMULATION
	if (!smack_fs_mount_point()) {
		return -EACCES;
	}
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

int smaunch_prepare(char **keys)
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

	if (!setdef)
		return 0;

	result = smaunch_fs_context_add(default_fs_key);
	assert(!result);
	return result;
}

int smaunch_apply()
{
#if SIMULATION
	int result;

	assert(smaunch_is_ready());

	/* apply smack and fs rules */
	result = smaunch_smack_context_apply();
	if (!result) {
		result = smaunch_fs_context_apply();
	}

	return result;
#else
	int result, length, file;
	char slabel[256];

	assert(smaunch_is_ready());

	/* Open once the smack context control */
	file = open("/proc/self/attr/current", O_RDWR);
	if (file < 0)
		return -errno;

	/* Save the smack context */
	length = (int)pread(file, slabel, sizeof slabel - 1, 0);
	if (length < 0) {
		result = -errno;
		close(file);
		return result;
	}
	if (length == 0) {
		strncpy(slabel, "User", 5);
		length = 4;
	} else {
		slabel[length] = 0;
	}

	/* Set context to floor to be able to mount */
	result = pwrite(file, "_", 1, 0);
	if (result < 0) {
		result = -errno;
		close(file);
		return result;
	}

	/* apply smack and fs rules */
	smaunch_smack_set_subject(slabel);
	result = smaunch_smack_context_apply();
	if (!result) {
		result = smaunch_fs_context_apply();
	}

	/* Restore the saved smack context */
	length = pwrite(file, slabel, length, 0);
	if (length < 0 && !result)
		result = -errno;

	close(file);

	return result;
#endif
}

int smaunch_exec(char **keys, const char *filename, char **argv, char **envp)
{
	int result;

	assert(smaunch_is_ready());

	result = smaunch_prepare(keys);
	if (result)
		return result;

	result = smaunch_apply();
	if (result)
		return result;

	result = execve(filename, argv, envp ? envp : environ);
	return -errno;
}

int smaunch_fork_exec(char **keys, const char *filename, char **argv, char **envp)
{
	volatile int result;
	pid_t cpid;

	assert(smaunch_is_ready());

	/* prepare the contexts */
	result = smaunch_prepare(keys);
	if (result)
		return result;

	/* using vfork is really what is wanted */
	cpid = vfork();
	if (cpid < 0) {
		return -errno;
	}

	if (!cpid) {
		/* within newly vforked child */
		result = smaunch_apply();
		if (!result) {
			execve(filename, argv, envp ? envp : environ);
			result = -errno;
		}
		_exit(1); /* using _exit instead of exit is MANDATORY for vfork */
	}

	/* within vforking parent, after that child exits or execs */
	assert(cpid > 0);
	return result;
}

