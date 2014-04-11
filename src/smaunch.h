/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * Returns the current path of the database for smack rules.
 */
const char *smaunch_get_database_smack_path();

/*
 * Returns the current path of the database for filesystem rules.
 */
const char *smaunch_get_database_fs_path();

/*
 * Returns the current default key for filesystem rules.
 */
const char *smaunch_get_default_fs_key();

/*
 * Set the path of the database of smack rules.
 *
 * It will take effect at the next init.
 *
 * Requires: smackdb != NULL
 *
 * Returns 0 on success or -ENOTSUP if the default key can't be changed.
 */
int smaunch_set_database_smack_path(const char *smackdb);

/*
 * Set the path of the database of filesystem rules.
 *
 * It will take effect at the next init.
 *
 * Requires: fsdb != NULL
 *
 * Returns 0 on success or -ENOTSUP if the default key can't be changed.
 */
int smaunch_set_database_fs_path(const char *fsdb);

/*
 * Set the default key for filesystem rules.
 *
 * Requires: key != NULL
 *
 * Ensure: !smaunch_is_ready()
 *
 * Returns 0 on success or -ENOTSUP if the default key can't be changed
 * or -ENOENT if smaunch_is_ready() and the key doesn't exist.
 */
int smaunch_set_default_fs_key(const char *key);

/*
 * Initializes Smaunch using the current databases and default key.
 *
 * Returns 0 on success or a negative error code on failure
 */
int smaunch_init();

/*
 * Tests if Smaunch is initialized and ready.
 *
 * Returns 1 if initialised and ready or 0 if not.
 */
int smaunch_is_ready();

/*
 * Prepares to apply the rules set by 'keys'.
 * The keys is an array of string pointers terminated by a NULL pointer.
 *
 * Requires: keys != NULL
 *           && smaunch_is_ready()
 *
 * Returns 0 on success or a negative error code on failure
 *
 * NOTE: That API verb will probably be removed
 */
int smaunch_prepare(const char **keys);

/*
 * Tests if Smaunch has prepared rules ready to apply.
 *
 * Returns 1 if prepared rules are ready or 0 if not.
 *
 * NOTE: That API verb will probably be removed
 */
int smaunch_has_prepared();

/*
 * Apply the rules prepared by the previous call
 * to 'smaunch_prepare'.
 *
 * Requires: keys != NULL
 *           && smaunch_is_ready()
 *           && smaunch_has_prepared()
 *
 * Returns 0 on success or a negative error code on failure
 *
 * NOTE: That API verb will probably be removed
 */
int smaunch_apply();

/*
 * Combines the following actions:
 *
 *  1. smaunch_prepare(keys)
 *  2. smaunch_apply()
 *  3. execve(filename, argv, envp)
 *
 * with the following subtleties:
 *  - error are handled
 *  - if envp == NULL the libc environment is used
 *
 * Requires: keys != NULL
 *           && filename != NULL
 *           && argv != NULL
 *           && smaunch_is_ready()
 *
 * Dont return in case of success.
 * Return a negative error code in case of failure.
 */
int smaunch_exec(const char **keys, const char *filename, char **argv, char **envp);

/*
 * Combines the following actions:
 *
 *  1. smaunch_prepare(keys)
 *  2. vfork
 *  3. (within vforked)
 *     3.1 smaunch_apply()
 *     3.2 execve(filename, argv, envp)
 *  4. (within caller)
 *     4.1 wait status (error/terminating or success/executed)
 *
 * with the following subtleties:
 *  - error are handled
 *  - if envp == NULL the libc environment is used
 *
 * Requires: keys != NULL
 *           && filename != NULL
 *           && argv != NULL
 *           && smaunch_is_ready()
 *
 * Return 0 if successfully forked and launched or a negative error code in case of failure.
 */
int smaunch_fork_exec(const char **keys, const char *filename, char **argv, char **envp);



