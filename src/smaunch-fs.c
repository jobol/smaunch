#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include "smaunch-fs.h"
#include "parse.h"
#include "buffer.h"

#define SIMULATION 0
#if SIMULATION
#include <stdio.h>
#define unshare(x)           (printf("unshare(%s)\n",#x), 0)
#define mount(f,t,k,c,x)     (printf("mount(%s, %s, %s, %s, %s)\n",f,t,k,#c,#x), 0)
#define mkdir(d,t)           (printf("mkdir(%s, %s)\n",d,#t), 0)
#endif

#define INVALID (-1)
#define ISVALID(x) ((x)>=0)
#define $(offset) (data.data + (offset))

enum key_offsets {
	key_string = 0,
	key_rule = 1,
	key_previous = 2,
	key_size = 3
};

enum dir_offsets {
	dir_string = 0,     /* the name */
	dir_parent = 1,     /* the parent */
	dir_child = 2,      /* first child if any */
	dir_depth = 3,      /* depth in the tree */
	dir_next = 4,       /* next child */
	dir_action = 5,     /* state for context */
	dir_size = 6
};

enum rule_offsets {
	rule_dir = 0,
	rule_kind = 1,
	rule_next = 2,
	rule_size = 3
};

enum positions {
	unrelated = 0,
	parent = 1, 
	child = 2, 
	equals = 3
};

enum mkinds { /* order cares */
	none = 0,
	read_only = 1,
	read_write = 2
};

enum actions {
	nothing = 0,
	should_exist,
	mount_none,
	mount_read_only,
	mount_read_write
};

static struct buffer data = { 0, 0, 0 };
static int last_key = INVALID;
static int root_dir = INVALID;
static int default_key = INVALID;
static const char const *(*substitutions)[2] = 0;
static int substitutions_count = 0;
static int unshared = 0;


static void clear_all()
{
	last_key = INVALID;
	root_dir = INVALID;
	default_key = INVALID;
	buffer_reinit(&data);
}

static int get_key(const char *key, int create)
{
	int result, istr;

	result = last_key;
	while (ISVALID(result)) {
		istr = $(result)[key_string];
		if (0 == strcmp(key, (char*)($(istr))))
			return result;
		result = $(result)[key_previous];
	}

	if (!create)
		return -ENOENT;

	istr = buffer_strdup(&data, key);
	if (istr < 0)
		return istr;

	result = buffer_alloc(&data, key_size);
	if (result < 0)
		return result;

	$(result)[key_string] = istr;
	$(result)[key_rule] = INVALID;
	$(result)[key_previous] = last_key;
	last_key = result;
	return result;
}

static int get_dir(const char **parts, int nparts, int create)
{
	int result, parent, depth, sts, istr;
	const char *name;

	assert(nparts > 0);

	if (!ISVALID(root_dir)) {
		if (!create)
			return -ENOENT;
		sts = buffer_alloc(&data, dir_size);
		if (sts < 0)
			return sts;
		root_dir = sts;
		$(sts)[dir_string] = INVALID;
		$(sts)[dir_parent] = INVALID;
		$(sts)[dir_child] = INVALID;
		$(sts)[dir_depth] = 0;
		$(sts)[dir_next] = INVALID;
		$(sts)[dir_action] = nothing;
	}

	depth = 0;
	result = root_dir;
	while (depth < nparts) {
		name = parts[depth++];
		parent = result;
		result = $(parent)[dir_child];
		for (;;) {
			if (!ISVALID(result)) {
				if (!create)
					return -ENOENT;
				istr = buffer_strdup(&data, name);
				if (istr < 0)
					return istr;
				result = buffer_alloc(&data, dir_size);
				if (result < 0)
					return result;
				$(result)[dir_string] = istr;
				$(result)[dir_parent] = parent;
				$(result)[dir_child] = INVALID;
				$(result)[dir_depth] = depth;
				$(result)[dir_next] = $(parent)[dir_child];
				$(result)[dir_action] = nothing;
				$(parent)[dir_child] = result;
				break;
			} else {
				istr = $(result)[dir_string];
				if (0 == strcmp(name, (char*)($(istr))))
					break;
				result = $(result)[dir_next];
			}
		}
	}

	return result;
}

static int split_path(char *path, const char **parts, int nparts)
{
	char *iter;
	int n, read, write;

	/* split */
	n = 0;
	iter = path;
	while (*iter) {
		while(*iter && *iter=='/') iter++;
		if (*iter) {
			if (n == nparts)
				return -E2BIG;
			parts[n++] = iter++;
			while(*iter && *iter!='/') iter++;
			if (*iter)
				*iter++ = 0;
		}
	}

	/* pack */
	read = 0;
	write = 0;
	while (read < n) {
		switch (parts[read][0]) {
		case 0: 
			break;
		case '.':
			switch (parts[read][1]) {
			case 0:
				break;
			case '.':
				if (!parts[read][2]) {
					if (write)
						write--;
					break;
				}
			default:
				parts[write++] = parts[read];
				break;
			}
			break;
		default:
			parts[write++] = parts[read];
			break;
		}
		read++;
	}

	return write;
}

static int get_dir_split(char *path, int create)
{
	int count;
	const char *parts[64];

	count = split_path(path, parts, sizeof parts / sizeof * parts);
	return count < 0 ? count : get_dir(parts, count, create);
}

static enum positions cmpdir(int dir1, int dir2)
{
	int d1, d2;

	assert(dir1 >= 0);
	assert(dir2 >= 0);

	if (dir1 == dir2)
		return equals;

	d1 = $(dir1)[dir_depth];
	d2 = $(dir2)[dir_depth];
	if (d1 > d2) {
		while (d1 > d2) {
			dir1 = $(dir1)[dir_parent];
			d1--;
		}
		if (dir1 == dir2)
			return child;
	} else {
		while (d1 < d2) {
			dir2 = $(dir2)[dir_parent];
			d2--;
		}
		if (dir1 == dir2)
			return parent;
	}
	return unrelated;
}

static int get_dir_path_rec(int diri, char *buffer, int length, int stopi)
{
	const char *name;
	int i, pari, result;

	assert(ISVALID(diri));
	assert(ISVALID(stopi));

	/* the root case */
	if (diri == root_dir) {
		if (length)
			buffer[0] = '/';
		return 1;
	}

	assert(ISVALID($(diri)[dir_string]));
	assert(ISVALID($(diri)[dir_parent]));

	/* the stop case */
	if (diri == stopi)
		return 0;

	/* compute the parent */
	pari = $(diri)[dir_parent];
	result = get_dir_path_rec(pari, buffer, length, stopi);

	/* get the substituted name */
	name = (const char *)$($(diri)[dir_string]);
	if (name[0] == '%') {
		i = substitutions_count;
		while (i) {
			if (0 == strcmp(name, substitutions[--i][0])) {
				name = substitutions[i][1];
				i = 0;
			}
		}
	}

	/* write now */
	if (pari != root_dir) {
		if (result < length)
			buffer[result] = '/';
		result++;
	}
	for (i = 0 ; name[i] ; i++) {
		if (result < length)
			buffer[result] = name[i];
		result++;
	}

	return result;
}

static int get_dir_path(int diri, char *buffer, int length, int stopi)
{
	int result;

	assert(ISVALID(diri));
	assert(ISVALID(stopi));
	assert(diri == stopi || cmpdir(diri, stopi) == child);
	assert(buffer || length <= 0);

	result = get_dir_path_rec(diri, buffer, length, stopi);
	if (result < length)
		buffer[result] = 0;

	return result;
}

static int add_rule(int keyi, int diri, enum mkinds kind)
{
	int iprvi, nrulei, rulei, stop, waschild;

	assert(keyi >= 0);
	assert(diri >= 0);

	/* search insertion point */
	iprvi = keyi + key_rule;
	waschild = 0;
	stop = 0;
	while (!stop) {
		nrulei = *$(iprvi);
		if (!ISVALID(nrulei))
			stop = 1;
		else {
			switch(cmpdir(diri, $(nrulei)[rule_dir])) {
			case parent:
				stop = 1;
				break;
			case child:
				waschild = 1;
				iprvi = nrulei + rule_next;
				break;
			case equals:
				if (kind < $(nrulei)[rule_kind])
					$(nrulei)[rule_kind] = kind;
				return 0;
			default:
				if (waschild)
					stop = 1;
				else
					iprvi = nrulei + rule_next;
				break;
			}
		}
	}

	/* allocation */
	rulei = buffer_alloc(&data, rule_size);
	if (rulei < 0)
		return rulei;

	/* initialisation */
	$(rulei)[rule_dir] = diri;
	$(rulei)[rule_kind] = kind;
	$(rulei)[rule_next] = nrulei;
	*$(iprvi) = rulei;

	return 0;
}

static int compile_database(const char *path)
{
	struct parse parse;
	int sts, keyi, diri;
	enum mkinds kind;

	/* open the file */
	sts = parse_init_open(&parse, path);
	if (sts < 0)
		return sts;

	/* parse the file */
	while(!parse.finished) {

		/* read one line */
		sts = parse_line(&parse);
		if (sts < 0) {
			close(parse.file);
			return sts;
		}

		switch (parse.keycount) {
		case 0:
			break;

		case 1: /* defines a key */
			if (parse.begsp) {
				close(parse.file);
				return parse_make_syntax_error(fs_directory_incomplete, parse.lino);
			}
			sts = get_key(parse.keys[0], 1);
			if (sts < 0) {
				close(parse.file);
				return sts;
			}
			keyi = sts;
			break;

		case 2: /* defines a directory */
			if (!parse.begsp) {
				close(parse.file);
				return parse_make_syntax_error(fs_directory_incomplete, parse.lino);
			}
			if (0 == strcmp(parse.keys[0], "-")) {
				kind = none;
			} else if (0 == strcmp(parse.keys[0], "+r")) {
				kind = read_only;
			} else if (0 == strcmp(parse.keys[0], "+rw")) {
				kind = read_write;
			} else {
				close(parse.file);
				return parse_make_syntax_error(fs_wrong_action, parse.lino);
			}
			if (parse.keys[1][0] != '/') {
				close(parse.file);
				return parse_make_syntax_error(fs_bad_directory, parse.lino);
			}
			sts = get_dir_split(parse.keys[1], 1);
			if (sts < 0) {
				close(parse.file);
				if (sts + E2BIG)
					return sts;
				return parse_make_syntax_error(fs_bad_directory_depth, parse.lino);
			}
			diri = sts;
			sts = add_rule(keyi, diri, kind);
			if (sts < 0) {
				close(parse.file);
				return sts;
			}
			break;

		default:
			close(parse.file);
			return parse_make_syntax_error(fs_too_many_fields, parse.lino);
		}
	}
	/* close the file */
	close(parse.file);

	return 0;
}

int smaunch_fs_load_database(const char *path)
{
	int result;

	clear_all();
	result = compile_database(path);
	if (result < 0)
		clear_all();

	return result;
}

int smaunch_fs_has_database()
{
	return ISVALID(last_key);
}

int smaunch_fs_has_key(const char *key)
{
	assert(key);
	assert(smaunch_fs_has_database());

	return ISVALID(get_key(key, 0));
}

int smaunch_fs_valid_substitutions(const char const *substs[][2], int count)
{
	int i;

	if (count < 0)
		return 0;
	if (count == 0)
		return 1;
	if (!substs)
		return 0;

	for(i = 0 ; i < count ; i++) {
		if (!substs[i][0])
			return 0;
		if (!substs[i][1])
			return 0;
		if (!*substs[i][0])
			return 0;
		if (!*substs[i][1])
			return 0;
		if (strchr(substs[i][0],'/') != NULL)
			return 0;
		if (strchr(substs[i][1],'/') != NULL)
			return 0;
	}

	for(i = 0 ; i < count ; i+=2)
		if (*substs[i][0] != '%')
			return 0;

	return 1;
}

void smaunch_fs_set_substitutions(const char const *substs[][2], int count)
{
	assert(smaunch_fs_valid_substitutions(substs, count));
	substitutions = substs;
	substitutions_count = count;
}













static void set_dir_childs_action_rec(int diri, enum actions action)
{
	int child;

	child = $(diri)[dir_child];
	while(ISVALID(child)) {
		$(child)[dir_action] = action;
		set_dir_childs_action_rec(child, action);
		child = $(child)[dir_next];
	}
}

static void set_dir_childs_action(int diri, enum actions action)
{
	if (ISVALID(diri))
		set_dir_childs_action_rec(diri, action);
}

static void prepare_dir(int diri, enum mkinds kind)
{
	enum actions action;

	action = mount_none + kind;
	if (action > $(diri)[dir_action]) {

		/* wider privilege */
		$(diri)[dir_action] = action;

		/* reset children */
		set_dir_childs_action(diri, nothing);

		/* force existing path */
		diri = $(diri)[dir_parent];
		while (ISVALID(diri) && $(diri)[dir_action] == nothing) {
			$(diri)[dir_action] = should_exist;
			diri = $(diri)[dir_parent];
		}
	}
}

static void prepare_key(int keyi)
{
	int rulei;

	rulei = $(keyi)[key_rule];
	while (ISVALID(rulei)) {
		prepare_dir($(rulei)[rule_dir], $(rulei)[rule_kind]);
		rulei = $(rulei)[rule_next];
	}
}


static int get_mount_fs(int diri, char *buffer, int length)
{
	int len;

	len = get_dir_path(diri, buffer, length, root_dir);
	if (len >= length)
		return -ENAMETOOLONG;

	return 0;
}

static int get_mount_tmp(int refdiri, int diri, char *buffer, int length)
{
	int len;

	assert(length >= 4);

	buffer[0] = '/';
	buffer[1] = 't';
	buffer[2] = 'm';
	buffer[3] = 'p';
	len = get_dir_path(diri, buffer+4, length, refdiri);
	if (len >= length - 4)
		return -ENAMETOOLONG;

	return 0;
}

static int get_mount_points(int refdiri, int diri, char *fs, char *tmp, int length)
{
	int result;

	result = get_mount_fs(diri, fs, length);
	if (!result)
		result = get_mount_tmp(refdiri, diri, tmp, length);

	return result;
}

static int apply_sub_dirs(int refdiri, int diri, enum actions refact);

static int apply_mount(int refdiri, int diri, enum actions refact, enum actions action)
{
	char tmpdir[PATH_MAX], fsdir[PATH_MAX];

	int result;
	int bind = action != mount_none;
	int ronly = action != mount_read_write;
	int move = refact == nothing;
	int apply = refact != action;

	if (!unshared) {
		result = unshare(CLONE_NEWNS);
		if (result < 0)
			return -errno;
		result = mount("", "/", "", MS_SLAVE|MS_REC, 0);
		if (result < 0)
			return -errno;
		unshared = 1;
	}

	result = get_mount_points(refdiri, diri, fsdir, tmpdir, PATH_MAX);
	if (result)
		return result;

	if (apply) {
		if (bind) {
			result = mount(fsdir, tmpdir, "", MS_BIND, 0);
		} else {
			result = mount("ram", tmpdir, "ramfs", 0, 0);
		}
		if (result < 0)
			return -errno;
	}

	result = apply_sub_dirs(refdiri, $(diri)[dir_child], action);
	if (result < 0)
		return result;

	if (apply && ronly) {
		result = mount("", tmpdir, "", MS_REMOUNT|MS_RDONLY, 0);
		if (result < 0)
			return -errno;
	}

	if (move) {
		result = mount(tmpdir, fsdir, "", MS_MOVE, 0);
	}

	return result;
}

static int apply_sub_dirs(int refdiri, int diri, enum actions refact)
{
	char tmpdir[PATH_MAX];
	int result, action;

	result = 0;
	while (!result && ISVALID(diri)) {
		action = $(diri)[dir_action];
		if (action != nothing) {
			result = get_mount_tmp(refdiri, diri, tmpdir, PATH_MAX);
			if (!result) {
				result = mkdir(tmpdir, 0777);
				result = !result || errno == EEXIST ? 0 : -errno;
			}
			if (!result) {
				switch (action) {
				case should_exist:
					result = apply_sub_dirs(refdiri, $(diri)[dir_child], refact);
					break;
				default:
					result = apply_mount(refdiri, diri, refact, action);
					break;
				}
			}
		}
		diri = $(diri)[dir_next];
	}
	return result;
}

static int apply_free_dirs(int diri)
{
	int result, action;

	result = 0;
	while (!result && ISVALID(diri)) {
		action = $(diri)[dir_action];
		switch (action) {
		case nothing:
			break;
		case should_exist:
			result = apply_free_dirs($(diri)[dir_child]);
			break;
		default:
			result = apply_mount(diri, diri, nothing, action);
			break;
		}
		diri = $(diri)[dir_next];
	}
	return result;
}







int smaunch_fs_context_start(const char *defaultkey)
{
	int keyi;

	assert(defaultkey);
	assert(smaunch_fs_has_database());

	keyi = get_key(defaultkey, 0);
	if (keyi < 0)
		return keyi;

	if (ISVALID(root_dir)) {
		$(root_dir)[dir_action] = nothing;
		set_dir_childs_action(root_dir, nothing);
	}

	default_key = keyi;
	return 0;
}

int smaunch_fs_context_add(const char *key)
{
	int keyi;

	assert(key);
	assert(smaunch_fs_has_database());

	keyi = get_key(key, 0);
	if (keyi < 0)
		return keyi;

	prepare_key(keyi);

	default_key = INVALID;
	return 0;
}

int smaunch_fs_context_apply()
{
	int result;

	assert(smaunch_fs_has_database());

	unshared = 0;

	if (!ISVALID(default_key)) {
		result = apply_free_dirs(root_dir);
	} else {
		prepare_key(default_key);
		result = apply_free_dirs(root_dir);
		if (ISVALID(root_dir)) {
			$(root_dir)[dir_action] = nothing;
			set_dir_childs_action(root_dir, nothing);
		}
	}
	return result;
}




#ifdef TEST_FS
#include <stdio.h>

const char *dir2str(int diri)
{
	static char tampon[PATH_MAX];
	get_dir_path(diri, tampon, (int)(sizeof tampon), root_dir);
	return tampon;
}

void dump_rules(int rulei)
{
	static char *kindnames[] = { "none", "read-only", "read-write" };
	while(ISVALID(rulei)) {
		printf("  RULE %-15s %s\n", kindnames[$(rulei)[rule_kind]], dir2str($(rulei)[rule_dir]));
		rulei = $(rulei)[rule_next];
	}
}

void dump_keys_rec(int keyi)
{
	if (ISVALID(keyi)) {
		dump_keys_rec($(keyi)[key_previous]);
		printf("KEY %s\n", (char*)($($(keyi)[key_string])));
		dump_rules($(keyi)[key_rule]);
		printf("\n");
	}
}

void dump_keys()
{
	dump_keys_rec(last_key);
}

void dump_dirs_rec(int diri)
{
	static char *actionames[] = { "", ".", "--", "ro", "rw" };
	if (ISVALID(diri)) {
		printf("DIR %3s %s\n", actionames[$(diri)[dir_action]], dir2str(diri));
		dump_dirs_rec($(diri)[dir_child]);
		dump_dirs_rec($(diri)[dir_next]);
	}
}

void dump_dirs()
{
	dump_dirs_rec(root_dir);
}

void dump_all()
{
	dump_keys();
	dump_dirs();
}

void apply_all_rec(int keyi)
{
	if (ISVALID(keyi)) {
		printf("\n\nAPPLYING %s\n",(char*)$($(keyi)[key_string]));
		smaunch_fs_context_start((char*)$($(keyi)[key_string]));
		smaunch_fs_context_add((char*)$($(keyi)[key_string]));
		dump_dirs();
		smaunch_fs_context_apply();
		system("/bin/sh");
		apply_all_rec($(keyi)[key_previous]);
	}
}

void apply_all()
{
	apply_all_rec(last_key);
}

int main(int argc, char **argv)
{
	int i, j, n;
	const char *parts[50];
	static const char *substs[][2] = { { "%user", "intel" }, { "%id", "<%ID>" } };
	for(i=1; i < argc ; i++) {
		n = split_path(argv[i], parts, 50);
		printf("[%d]", n);
		if (n >= 0)
			for(j = 0 ; j < n ; j++)
				printf(" / %s", parts[j]);
		printf("\n");
	}
	smaunch_fs_set_substitutions(substs, 2);
	n = smaunch_fs_load_database("dbfs");
	printf("\nLOADING dbfs: %d\n", n);
	if (!n) {
		dump_all();
		apply_all();
	}
	
	return 0;
}
#endif
