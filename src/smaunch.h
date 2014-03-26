/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

int smaunch_init(const char *smackdb, const char *fsdb, const char *defskey);

int smaunch_is_ready();

int smaunch_prepare(char **keys);

int smaunch_apply();

int smaunch_exec(char **keys, const char *filename, char **argv, char **envp);

int smaunch_fork_exec(char **keys, const char *filename, char **argv, char **envp);



