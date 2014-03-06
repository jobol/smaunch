/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

int smaunch_smack_load_database(const char *path);
int smaunch_smack_has_database();
int smaunch_smack_has_key(const char *key);
void smaunch_smack_context_start();
int smaunch_smack_context_add(const char *key);
int smaunch_smack_context_apply();
int smaunch_smack_dump_all(int file);

enum smaunch_smack_syntax_errors {
	smack_extra_after_key = 3,
	smack_object_without_access = 4,
	smack_invalid_object = 5,
	smack_extra_after_access = 6,
	smack_no_key_set = 7,
	smack_invalid_access = 8
};

