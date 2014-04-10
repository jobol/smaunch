/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <attr/xattr.h>

#include "launch-spec.h"

static const char usage_text[] = 
"smaunch-attr reading attribute\n"
"\n"
" smaunch-attr files...\n"
"\n"
"smaunch-attr setting attribute\n"
"\n"
" smaunch-attr -s attr files...\n"
"\n"
;

static int get(char **files)
{
	int nerr;
	char buffer[16384];
	int sts;
	int len;
	struct launch_spec spec;

	nerr = 0;
	while (*files) {
		printf("%s:\n", *files);
		len = lgetxattr(*files, SMAUNCH_LAUNCHER_XATTR_NAME, buffer, sizeof buffer - 1);
		if (len < 0) {
			if (errno != ENODATA) {
				fprintf(stderr, "error while reading attributes of %s: %s\n", *files, strerror(errno));
				nerr++;
			}
		} else {
			printf("%.*s", len, buffer);
			if (!len || buffer[len-1] != '\n')
				printf("\n");
			buffer[len] = 0;
			launch_spec_init(&spec);
			sts = launch_spec_parse(&spec, buffer);
			if (sts) {
				fprintf(stderr, "error while validating attributes of %s: %s\n", *files, strerror(-sts));
				nerr++;
			}
		}
		printf("\n");
		files++;
	}
	return nerr;
}

static int set(char *data, char **files)
{
	int nerr;
	int sts;
	size_t len;
	struct launch_spec spec;
	char buffer[16384];

	len = strlen(data);
	if (len >= sizeof buffer) {
		fprintf(stderr, "error while validating value: too long\n");
		return 1;
	}
	strcpy(buffer, data);
	launch_spec_init(&spec);
	sts = launch_spec_parse(&spec, buffer);
	if (sts) {
		fprintf(stderr, "error while validating value: %s\n", strerror(errno));
		return 1;
	}

	nerr = 0;
	while (*files) {
		len = lsetxattr(*files, SMAUNCH_LAUNCHER_XATTR_NAME, data, len, 0);
		if (len < 0) {
			fprintf(stderr, "error while setting attributes of %s: %s\n", *files, strerror(errno));
			nerr++;
		}
		files++;
	}
	return nerr;
}

int main(int argc, char **argv, char **env)
{
	if (!argv[1] || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
		printf("%s", usage_text);
		return 0;
	}

	if (!strcmp(argv[1], "-s"))
		return set(argv[2], argv+3);

	if (argv[1][0] == '-') {
		printf("%s", usage_text);
		return 1;
	}

	return get(argv+1);
}


