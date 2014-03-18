/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * Check the validity of the smack label of 'text'.
 *
 * Requires: text != NULL
 *
 * Returns 1 if valid or 0 otherwise.
 */
int smack_label_is_valid(const char *text);

/*
 * Check the validity of the smack subject label of 'text'.
 *
 * Requires: text != NULL
 *
 * Returns 1 if valid or 0 otherwise.
 */
int smack_subject_is_valid(const char *text);

/*
 * Check the validity of the smack object label of 'text'.
 *
 * Requires: text != NULL
 *
 * Returns 1 if valid or 0 otherwise.
 */
int smack_object_is_valid(const char *text);

