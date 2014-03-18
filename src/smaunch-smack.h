/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * error codes for parsing
 */
enum smaunch_smack_syntax_errors {
    smack_line_too_long = 1,			/* line too long */
	smack_too_much_fields = 2,			/* too many fields */
	smack_extra_after_key = 3,			/* extra field after key */
	smack_object_without_access = 4,	/* no permission set for the object */
	smack_invalid_object = 5,			/* invalid object label */
	smack_extra_after_access = 6,		/* extra field after permission */
	smack_no_key_set = 7,				/* object without key context */
	smack_invalid_access = 8			/* invalid permission spec */
};

/*
 * Set the 'subject' of the rules.
 *
 * CAUTION: THE POINTER IS RECORDED, THERE IS NO COPY
 *
 * Requires: subject != NULL
 *           && smack_subject_is_valid(subject)
 */
void smaunch_smack_set_subject(const char *subject);

/*
 * Load the database of 'path'.
 *
 * Requires: path != NULL
 *
 * Returns 0 on success or a negative error code otherwise.
 */
int smaunch_smack_load_database(const char *path);

/*
 * Test if a database is loaded.
 *
 * Returns 1 if database is loaded, 0 otherwise.
 */
int smaunch_smack_has_database();

/*
 * Test if the key is in the database.
 *
 * Requires: key != NULL
 *           && smaunch_smack_has_database()
 *
 * Returns 1 if the key exists or 0 otherwise.
 */
int smaunch_smack_has_key(const char *key);

/*
 * Start a new context definition.
 */
void smaunch_smack_context_start();

/*
 * Add the granted 'key' to the current context.
 *
 * Requires: key != NULL
 *           && smaunch_smack_has_database()
 *
 * Returns 0 on success or a negative value on error.
 * In particular, the return value -ENOENT is returned
 * if the key ins't in the database (if !smaunch_smack_has_key(key)).
 */
int smaunch_smack_context_add(const char *key);

/*
 * Apply the current context to the smack process context
 * using load-self2 interface.
 *
 * Requires: smaunch_smack_has_database()
 *
 * Returns 0 on success or a negative value on error.
 */
int smaunch_smack_context_apply();

/*
 * Dump the default "all granted" configuration to 'file'.
 *
 * Requires: smaunch_smack_has_database()
 *
 * Returns 0 on success or a negative value on error.
 */
int smaunch_smack_dump_all(int file);

