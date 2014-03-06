/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
typedef int smack_coda;

enum {
	smack_coda_l = 1,
	smack_coda_t = 2,
	smack_coda_a = 4,
	smack_coda_x = 8,
	smack_coda_w = 16,
	smack_coda_r = 32,
	smack_coda_max = 32,
	smack_coda_bits_count = 6,
	smack_coda_bits_mask = 0x3f
};

/*
Is 'text' a valid string for coda?

'text' MUST not be NULL.

Returns: 1 if yes, the access is valid, 0 otherwise.
*/
smack_coda smack_coda_string_is_valid(const char *text);
int smack_coda_is_valid(smack_coda coda);
smack_coda smack_coda_complement(smack_coda coda);
smack_coda smack_coda_from_string(const char *text);
int smack_coda_string_length(smack_coda coda);
int smack_coda_to_string(smack_coda coda, char *text, int length);

