/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

int smaunch_fs_load_database(const char *path);
int smaunch_fs_has_database();
int smaunch_fs_has_key(const char *key);

int smaunch_fs_valid_substitutions(const char const *substs[][2], int count);
void smaunch_fs_set_substitutions(const char const *substs[][2], int count);

int smaunch_fs_context_start(const char *defaultkey);
int smaunch_fs_context_add(const char *key);
int smaunch_fs_context_apply();

enum smaunch_fs_syntax_errors {
	fs_directory_incomplete = 3,
	fs_extra_after_key = 4,
	fs_wrong_action = 5,
	fs_bad_directory = 6,
	fs_bad_directory_depth = 7,
	fs_too_many_fields = 8
};


