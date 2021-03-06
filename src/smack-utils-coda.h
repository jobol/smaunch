/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * A smack_coda is a smack code of access and it is an integer data.
 */
typedef int smack_coda;

/*
 * Constants used for smack_coda
 */
enum {
	smack_coda_lock = 1,			/* lock permission bit */
	smack_coda_transmute = 2,		/* transmute permission bit */
	smack_coda_append = 4,			/* append permission bit */
	smack_coda_execute = 8,			/* execute permission bit */
	smack_coda_write = 16,			/* write permission bit */
	smack_coda_read = 32,			/* read permission bit */
	smack_coda_bits_count = 6,		/* count of bits for permissions */
	smack_coda_bits_mask = 0x3f		/* mask of the permissions bits */
};

/*
 * Is 'text' a valid string for coda?
 *
 * Requires: text != NULL
 *
 * Returns: 1 if yes, the access is valid, 0 otherwise.
*/
int smack_coda_string_is_valid(const char *text);

/*
 * Is 'coda' valid?
 *
 * Returns: 1 if yes, the access is valid, 0 otherwise.
 */
int smack_coda_is_valid(smack_coda coda);

/*
 * Returns the normalized coda value for the string 'text'.
 *
 * Requires: text != NULL
 *           && smack_coda_string_is_valid(text)
 */
smack_coda smack_coda_from_string(const char *text);

/*
 * Returns the length (excluding the terminating null) of the 
 * string representation of 'coda'.
 *
 * Requires: smack_coda_is_valid(coda)
 */
int smack_coda_string_length(smack_coda coda);

/*
 * Put the string representation of 'coda' in 'text' without the terminating
 * null. The available 'length' of 'text' must be enough to handle the string
 * representation.
 *
 * CAUTION: NOT NUL IS APPENED
 *
 * Requires: smack_coda_is_valid(coda)
 *           && text != NULL
 *           && length >= smack_coda_string_length(coda)
 *
 * Returns the count of characters written to 'text'.
 */
int smack_coda_to_string(smack_coda coda, char *text, int length);

/*
 * Returns the complement of 'coda'.
 *
 * Requires: smack_coda_is_valid(coda)
 */
smack_coda smack_coda_complement(smack_coda coda);

/*
 * Returns 'coda' modified such that the lock permission
 * is added if 'coda' has the write permission.
 *
 * Requires: smack_coda_is_valid(coda)
 *
 * Returns the normalized coda.
 */
smack_coda smack_coda_normalize(smack_coda coda);

/*
 * Check if the 'coda' is normalized what means that the write
 * implies the lock permission.
 *
 * Requires: smack_coda_is_valid(coda)
 *
 * Returns 1 if 'coda' is normal or 0 otherwise.
 */
int smack_coda_is_normal(smack_coda coda);

