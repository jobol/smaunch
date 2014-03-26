/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "smaunch-smack.h"
#include "smaunch-fs.h"
#include "smaunch.h"








#if !defined(MEASURETIMES)
#define MEASURETIMES 1
#endif

#if !defined(ALLOWOVERRIDE)
#define ALLOWOVERRIDE 1
#endif




#if !defined(SMAUNCH_DB_DIR)
#define SMAUNCH_DB_DIR		"/etc"
#endif

#if !defined(SMAUNCH_DB_NAME)
#define SMAUNCH_DB_NAME		"smaunch"
#endif

#if !defined(SMAUNCH_DEFKEY)
#define SMAUNCH_DEFKEY		"restricted"
#endif

#define DB_SMACK_TEXTUAL	SMAUNCH_DB_DIR "/" SMAUNCH_DB_NAME ".smack"
#define DB_SMACK_COMPILED	SMAUNCH_DB_DIR "/." SMAUNCH_DB_NAME ".smack.bin"

#define DB_FS_TEXTUAL		SMAUNCH_DB_DIR "/" SMAUNCH_DB_NAME ".fs"
#define DB_FS_COMPILED		SMAUNCH_DB_DIR "/." SMAUNCH_DB_NAME ".fs.bin"

#define DEFAULT_KEY			SMAUNCH_DEFKEY

#define MAX_SUBST_COUNT		64

static char usage_text[] =
"smaunch compiling\n"
"\n"
" smaunch -cf --compile-fs     [infile [outfile]]\n"
" smaunch -cs --compile-smack  [infile [outfile]]\n"
"\n"
"smaunch checking\n"
"\n"
" smaunch -Cf --check-fs       [infile]\n"
" smaunch -Cs --check-smack    [infile]\n"
"\n"
"smaunch launcher\n"
"\n"
#if ALLOWOVERRIDE
" smaunch options... [%substitutions...] [keys...] [-- command args]\n"
"\n"
" opt: -df --db-fs     path    (default: "DB_FS_COMPILED")\n"
"      -ds --db-smack  path    (default: "DB_SMACK_COMPILED")\n"
"      -k  --defkey    key     (default: "DEFAULT_KEY"\n"
#else
" smaunch [%substitutions...] [keys...] [-- command args]\n"
#endif
;

static const char *substs[MAX_SUBST_COUNT][2];
static int nsubsts = 0;
static char *default_command[] = { "/bin/sh", NULL };

#if MEASURETIMES

#include <sys/time.h>

static struct timeval  tops[20];

#define top(x)     gettimeofday(&tops[x],NULL)
#define tus(x)     ((double)(tops[x].tv_sec) * (double)1e6 + (double)(tops[x].tv_usec))
#define dur(x,y)   (tus(y) - tus(x))
#define ptm(...)   fprintf(stderr, __VA_ARGS__)
#define pdu(t,x,y) ptm("timing: %10s = %9.3lf ms\n", t, dur(x,y)/1000)

#else

#define top(x)
#define ptm(...)
#define pdu(t,x,y)

#endif

static int usage(int result)
{
	fprintf(result ? stderr : stdout, "%s", usage_text);
	return result;
}

static int error(int err, const char *context, const char *dbsmack, const char *dbfs)
{
	return 1;
}

static int compile(char **argv, int (*load)(const char*), int (*save)(const char*))
{
	int status;
	const char *inpath, *outpath;
	static const char stdinput[] = "/dev/stdin";
	static const char stdoutput[] = "/dev/stdout";

	top(0);

	/* get the paths */
	inpath = *argv;
	if (!inpath) {
		inpath = stdinput;
		outpath = stdoutput;
	} else {
		if (inpath[0] == '-' && !inpath[1])
			inpath = stdinput;
		outpath = *++argv;
		if (!outpath)
			outpath = stdoutput;
		else {
			argv++;
			if (outpath[0] == '-' && !outpath[1])
				outpath = stdoutput;
		
		}
	}

	/* load now */
	top(1);
	status = load(inpath);
	top(2);
	if (status)
		return error(status, save ? "loading database" : "checking database", inpath, inpath);

	/* save compiled now */
	if (save) {
		top(3);
		status = save(outpath);
		top(4);
		if (status)
			return error(status, "saving compiled database", outpath, outpath);
	}

	top(5);

	pdu("load", 1, 2);
	if (save) pdu("save", 3, 4);
	pdu("all", 0, 5);

	return 0;
}

static int addsubpair(const char *pattern, const char *replacement)
{
	int i;

	/* validate */
	switch(smaunch_fs_check_substitution_pair(pattern, replacement)) {
	case fs_substitution_is_valid:
		break;
	case fs_substitution_pattern_is_null:
	case fs_substitution_pattern_hasnt_percent:
	case fs_substitution_pattern_is_percent:
	case fs_substitution_pattern_has_slash:
	case fs_substitution_replacement_is_null:
	case fs_substitution_replacement_is_empty:
	case fs_substitution_replacement_has_slash:
	default:
		return 1;
	}

	/* replace if exists */
	i = nsubsts;
	while (i)
		if (!strcmp(substs[--i][0], pattern)) {
#if ALLOWOVERRIDE
			substs[i][1] = replacement;
			return 0;
#else
			return 1;
#endif
		}

	/* add */
	if (nsubsts == MAX_SUBST_COUNT)
		return 1;
	substs[nsubsts][0] = pattern;
	substs[nsubsts][1] = replacement;
	nsubsts++;
	return 0;
}

static int addsub(char *subst)
{
	char *iter = subst;
	while (*iter)
		if (*iter++ == '=') {
			iter[-1] = 0;
			return addsubpair(subst, iter);
		}
	return 1;
}

static int addsubdef()
{
/*
	char buffer[256];
*/
	if (addsubpair("%user", getenv("USER")))
		return 1;
	return 0;
}

static int launch(char** argv, char **env)
{
	int status;
	char *dbfs = DB_FS_COMPILED;
	char *dbsmack = DB_SMACK_COMPILED;
	char *defkey = DEFAULT_KEY;
	char **keys;
	char **command;

	top(0);

#if ALLOWOVERRIDE
	/* get options */
	while (argv[0] && argv[0][0] == '-' && (argv[0][1] != '-' || argv[0][2])) {
		if (!strcmp(*argv, "-df") || !strcmp(*argv, "--db-fs")) {
			dbfs = *++argv;
			if (!dbfs)
				return 1;
		} else if (!strcmp(*argv, "-ds") || !strcmp(*argv, "--db-smack")) {
			dbsmack = *++argv;
			if (!dbsmack)
				return 1;
		} else if (!strcmp(*argv, "-k") || !strcmp(*argv, "--defkey")) {
			defkey = *++argv;
			if (!defkey)
				return 1;
		} else
			return usage(1);
		argv++;
	}
#endif

	/* set the substitutions */
	if (addsubdef())
		return 1;
	while (argv[0] && argv[0][0] == '%') {
		if (addsub(*argv++))
			return 1;
	}
	smaunch_fs_set_substitutions(substs, nsubsts);

	/* set keys */
	keys = argv;
	if (argv[0] && strcmp(argv[0], "--")) {
		while (argv[0] && strcmp(argv[0], "--"))
			if (!**argv++)
				return usage(1);
	} else {
		while (*keys)
			keys++;
	}

	/* set the command */
	if (argv[0])
		argv++;
	command = argv[0] ? argv : default_command;

	/* invocation now */

	top(1);
	status = smaunch_init(dbsmack, dbfs, defkey);
	top(2);
	if (status)
		return error(status, "initializing", dbsmack, dbfs);

	top(3);
#if MEASURETIMES
	status = smaunch_fork_exec(keys, command[0], command, env);
#else
	status = smaunch_exec(keys, command[0], command, env);
#endif
	top(4);
	if (status)
		return error(status, "launching", dbsmack, dbfs);

	top(5);

	pdu("init", 1, 2);
	pdu("launch", 3, 4);
	pdu("all", 0, 5);

	wait(&status);

	return 0;
}



int main(int argc, char** argv, char **env)
{
	char **arg;

	arg = argv+1;

	if (!strcmp(*arg, "-h") || !strcmp(*arg, "--help"))
		return usage(0);

	if (!strcmp(*arg, "-cf") || !strcmp(*arg, "--compile-fs"))
		return compile(++arg, smaunch_fs_load_database, smaunch_fs_save_database_compiled);

	if (!strcmp(*arg, "-cs") || !strcmp(*arg, "--compile-smack"))
		return compile(++arg, smaunch_smack_load_database, smaunch_smack_save_database_compiled);

	if (!strcmp(*arg, "-Cf") || !strcmp(*arg, "--check-fs"))
		return compile(++arg, smaunch_fs_load_database, 0);

	if (!strcmp(*arg, "-Cs") || !strcmp(*arg, "--check-smack"))
		return compile(++arg, smaunch_smack_load_database, 0);

	return launch(arg, env);
}

