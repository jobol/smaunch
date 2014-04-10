/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * error codes reading databases
 */
enum smaunch_fs_syntax_errors {
	fs_line_too_long = 200,		/* line too long, see {parse_constants}.parse_line_length */
	fs_too_much_fields,			/* too much fields, see {parse_constants}.parse_field_count */
	fs_directory_incomplete,	/* missing permission or directory in a rule */
	fs_extra_after_key,			/* extra field after the key */
	fs_wrong_permission,		/* unknown permission */
	fs_bad_directory,			/* the directory isn't absolute */
	fs_bad_directory_depth,		/* too many sub directories */
	fs_too_many_fields,			/* too much fields found */
	fs_no_key_set,				/* rule without key */
	fs_file_empty,				/* no key set */
	fs_root_directory,			/* root directory is forbiden */
};

/*
 * Definition of substitution check codes
 */
enum smaunch_fs_substitution_check_code {
	fs_substitution_is_valid = 0,
	fs_substitution_pattern_is_null,
	fs_substitution_pattern_hasnt_percent,
	fs_substitution_pattern_is_percent,
	fs_substitution_pattern_has_slash,
	fs_substitution_replacement_is_null,
	fs_substitution_replacement_is_empty,
	fs_substitution_replacement_has_slash
};

/*
 * Returns a string for the given check 'code'.
 */
const char *smaunch_fs_substitution_check_code_string(enum smaunch_fs_substitution_check_code code);

/*
 * Tests if the substitution defined by 'pattern' and 'replacement'
 * is a valid substition pair.
 *
 * Substitutions are valid if all the following conditions are true:
 *    + pattern != NULL                   - pattern isn't null
 *    + replacement != NULL               - replacement isn't null
 *    + pattern[0] == '%'                 - pattern starts with %
 *    + pattern[1] != '\0'                - pattern isn't "%"
 *    + replacement[0] != '\0'            - replacement not empty
 *    + strchr(pattern, '/') == NULL      - pattern doesn't contain /
 *    + strchr(replacement, '/') == NULL  - replacement doesn't contain /
 *
 * Returns the matching substitution check code
 */
enum smaunch_fs_substitution_check_code smaunch_fs_check_substitution_pair(const char const *pattern, const char const *replacement);

/*
 * Tests if the substitutions defined by 'substs' and 'count'
 * are valid.
 *
 * Substitutions are valid if all the following conditions are true:
 *  - count >= 0
 *  - count != 0 implies substs != NULL
 *  - foreach i in 0 .. count-1
 *    + substs[i][0] != NULL
 *    + substs[i][1] != NULL
 *    + substs[i][0][0] == '%'             (starts with %)
 *    + substs[i][1][0] != '\0'            (not empty)
 *    + strchr(substs[i][0], '/') == NULL  (doesn't contain /)
 *    + strchr(substs[i][1], '/') == NULL  (doesn't contain /)
 *
 * Returns 1 if the substitution is valid 0 otherwise.
 */
int smaunch_fs_valid_substitutions(const char const *substs[][2], int count);

/*
 * Sets the substitutions to use.
 * The substitution is the 'substs' array of 'count' arrayed-pairs of strings.
 * Each pair is made with a pattern starting with '%' at index 0 and its
 * substituted value at index 1.
 *
 * Requires: smaunch_fs_valid_substitutions(substs, count)
 */
void smaunch_fs_set_substitutions(const char const *substs[][2], int count);

/*
 * Tests if a database is loaded and ready.
 *
 * Returns 1 if it is the case, 0 otherwise.
 */
int smaunch_fs_has_database();

/*
 * Loads the database of 'path'.
 *
 * Requires: path != NULL
 *
 * Return 0 on success or a negative error code otherwise.
 */
int smaunch_fs_load_database(const char *path);

/*
 * Saves the compiled version of database to 'path'.
 *
 * Requires: path != NULL
 *           && smaunch_fs_has_database()
 *
 * Return 0 on success or a negative error code otherwise.
 */
int smaunch_fs_save_database_compiled(const char *path);

/*
 * Tests if the key is in the database.
 *
 * Requires: key != NULL
 *           && smaunch_fs_has_database()
 *
 * Returns 1 if the key exists or 0 otherwise.
 */
int smaunch_fs_has_key(const char *key);

/*
 * Starts a new context definition.
 */
void smaunch_fs_context_start();

/*
 * Add the granted 'key' to the current context.
 *
 * Requires: key != NULL
 *           && smaunch_fs_has_database()
 *
 * Returns 0 on success or a negative value on error.
 * In particular, the return value -ENOENT is returned
 * if the key ins't in the database (if !smaunch_fs_has_key(key)).
 */
int smaunch_fs_context_add(const char *key);

/*
 * Apply the current context to the file system.
 *
 * Requires: smaunch_fs_has_database()
 *
 * Returns 0 on success or a negative value on error.
 */
int smaunch_fs_context_apply();

