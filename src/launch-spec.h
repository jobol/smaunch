/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#define SMAUNCH_LAUNCHER_XATTR_NAME    "security.smaunch-launcher"

enum {
	launch_spec_max_key_count = 128,
	launch_spec_max_subst_count = 128,
};

struct launch_spec {
	int nkeys;
	int nsubsts;
	const char *exec_target;
	const char *keys[launch_spec_max_subst_count];
	const char *substs[launch_spec_max_subst_count][2];
};

void launch_spec_init(struct launch_spec *spec);

int launch_spec_parse(struct launch_spec *spec, char *buffer);

int launch_spec_generate(struct launch_spec *spec, char **buffer, int *length);

int launch_spec_get_keys(struct launch_spec *spec, const char **keys, int count);

