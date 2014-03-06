/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
typedef int smack_access;

enum {
	smack_access_mask = 0x3f,
	smack_access_allow_mask = 0x3f,
	smack_access_deny_mask = 0xfc0,
	smack_access_shift_count = 6,
	smack_access_full_mask = 0xfff
};

int smack_access_is_valid(smack_access access);
smack_coda smack_access_allow_coda(smack_access access);
smack_coda smack_access_deny_coda(smack_access access);
smack_access smack_access_make(smack_coda allow, smack_coda deny);
smack_access smack_access_compose(smack_access first, smack_access second);
int smack_access_strings_are_valid(const char *allow, const char *deny);
smack_access smack_access_from_strings(const char *allow, const char *deny);
int smack_access_is_partial(smack_access access);
int smack_access_string_length(smack_access access);
int smack_access_to_string(smack_access access, char *text, int length);
smack_access smack_access_fullfill(smack_access access);



