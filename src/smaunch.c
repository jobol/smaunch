/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/prctl.h>
#include <linux/securebits.h>

#include "smaunch-smack.h"
#include "smaunch-fs.h"

int smaunch_init(const char *smackdb, const char *fsdb, const char *defskey)
{
	int result;

	if (smackdb) {
		result = smaunch_smack_load_database(smackdb);
		if (result < 0) /* todo: syntax error? */
			return result;
	}

	if (fsdb) {
		if (!defskey)
			return -EINVAL;
		result = smaunch_fs_load_database(fsdb);
		if (result < 0) /* todo: syntax error? */
			return result;
		result = smaunch_fs_context_start(defskey);
	}
	return result;
}

int smaunch_apply()
{
	int result;

	if (smaunch_fs_has_database()) {
		result = smaunch_fs_context_apply();
		if (result < 0)
			return result;
	}

	if (smaunch_smack_has_database()) {
		result = smaunch_smack_context_apply();
		if (result < 0)
			return result;
	}

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
	const char *substs[][2] = { { "%user", "jb" } };

	smaunch_fs_set_substitutions(substs, 1);

	sts = smaunch_init("db.smack", "db.fs", "restricted");
	printf("init %d\n", sts);

	sts = smaunch_smack_context_add("calendar.write");
	printf("add %d\n", sts);

	sts = smaunch_smack_context_add("WRT");
	printf("add %d\n", sts);

	sts = smaunch_apply();
	printf("apply %d\n", sts);
#endif
	return 0;
}
#endif

