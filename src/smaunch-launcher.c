/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <attr/xattr.h>

#include "parse.h"
#include "smaunch-smack.h"
#include "smaunch-fs.h"
#include "smaunch.h"

#include "keyzen.h"
#include "launch-spec.h"

static int serror(int err, const char *context)
{
	fprintf(stderr, "Error %d while %s: %s\n", err, context, strerror(-err));
	return 1;
}

static int error(int err, const char *context)
{
	if (parse_is_syntax_error(err)) {
		/* syntax error */
		int line;
		const char *file, *text, *dbsmack, *dbfs;

		dbsmack = smaunch_get_database_smack_path();
		dbfs = smaunch_get_database_fs_path();
		line = parse_syntax_error_line(err);
		switch (parse_syntax_error_number(err)) {

		/* smack database errors */
		case smack_line_too_long: file = dbsmack; text = "line too long"; break;
		case smack_too_much_fields: file = dbsmack; text = "too much fields"; break;
		case smack_extra_after_key: file = dbsmack; text = "extra field after key"; break;
		case smack_object_without_access: file = dbsmack; text = "no permission set for the object"; break;
		case smack_invalid_object: file = dbsmack; text = "invalid object label"; break;
		case smack_extra_after_access: file = dbsmack; text = "extra field after permission"; break;
		case smack_no_key_set: file = dbsmack; text = "object without key context"; break;
		case smack_invalid_access: file = dbsmack; text = "invalid permission spec"; break;
		case smack_file_empty: file = dbsmack; text = "the file has no data"; break;

		/* fs database errors */
		case fs_line_too_long: file = dbfs; text = "line too long"; break;
		case fs_too_much_fields: file = dbfs; text = "too much fields"; break;
		case fs_directory_incomplete: file = dbfs; text = "missing permission or directory in a rule"; break;
		case fs_extra_after_key: file = dbfs; text = "extra field after the key"; break;
		case fs_wrong_permission: file = dbfs; text = "unknown permission"; break;
		case fs_bad_directory: file = dbfs; text = "the directory isn't absolute"; break;
		case fs_bad_directory_depth: file = dbfs; text = "too many sub directories"; break;
		case fs_too_many_fields: file = dbfs; text = "too much fields found"; break;
		case fs_no_key_set: file = dbfs; text = "rule without key"; break;
		case fs_file_empty: file = dbfs; text = "no key set"; break;
		case fs_root_directory: file = dbfs; text = "root directory is forbiden"; break;

		/* ??? */
		default: file = "?"; text = file; break;
		}
		fprintf(stderr, "Syntax error %d while %s file %s line %d: %s\n", err, context, file, line, text);
		return 1;
	}
	return serror(err, context);
}

static void add_predef_substs(struct launch_spec *spec)
{
	assert(spec->nsubsts + 10 < launch_spec_max_subst_count);
	spec->substs[spec->nsubsts][0] = "%user";
	spec->substs[spec->nsubsts][1] = getenv("USER");
	spec->nsubsts++;
}

int main(int argc, char **argv, char **env)
{
	char buffer[16384];
	int nkeys;
	int sts;
	int len;
	struct launch_spec spec;
	const char *keys[launch_spec_max_subst_count+1];

	sts = readlink(argv[0], buffer, sizeof buffer);
	if (sts < 0)
		return serror(-errno, "checking that is a link");

	len = lgetxattr(argv[0], SMAUNCH_LAUNCHER_XATTR_NAME, buffer, sizeof buffer - 1);
	if (len < 0)
		return serror(-errno, "reading the context");

	buffer[len] = 0;
	launch_spec_init(&spec);
    add_predef_substs(&spec);
	sts = launch_spec_parse(&spec, buffer);
	if (sts)
		return serror(sts, "parsing the launch spec");

	sts = smaunch_fs_valid_substitutions(spec.substs, spec.nsubsts);
	if (!sts)
		return serror(-EINVAL, "validating substitutions");
	smaunch_fs_set_substitutions(spec.substs, spec.nsubsts);

	nkeys = launch_spec_get_keys(&spec, keys, sizeof keys / sizeof keys[0]);
	if (nkeys < 0)
		return serror(nkeys, "preparing the keys");

	sts = keyzen_self_set_keys(keys, nkeys);
	if (sts)
		return serror(sts, "setting the keys to keyzen");

	sts = smaunch_init();
	if (sts)
		return error(sts, "initialising smaunch");

	sts = smaunch_exec(keys, spec.exec_target, argv, env);
	assert(sts);
	return serror(sts, "launching");
}


