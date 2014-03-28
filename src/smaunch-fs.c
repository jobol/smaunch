/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#define _GNU_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/uio.h>

#include "smaunch-fs.h"
#include "parse.h"
#include "buffer.h"

#if !defined(SIMULATION)
#define SIMULATION 0
#endif

#if SIMULATION
#include <stdio.h>

#define mflgr(x,v,o) (((x)==(v)) ? #v : (o))
#define mflg(x)     mflgr(x,MS_BIND,\
					mflgr(x,MS_MOVE,\
					mflgr(x,MS_SLAVE|MS_REC,\
					mflgr(x,MS_REMOUNT|MS_RDONLY,\
					mflgr(x,MS_BIND|MS_REMOUNT|MS_RDONLY,\
					mflgr(x,0,\
					"???"))))))
#define unshare(x)           (printf("unshare(%s)\n",#x), 0)
#define mount(f,t,k,c,x)     (printf("mount(%s, %s, %s, %s, %s)\n",f,t,k,mflg(c),#x), 0)
#define mkdir(d,t)           (printf("mkdir(%s, %s)\n",d,#t), 0)
#endif

/* for compilation of databases */
#if !defined(ALLOWCOMPILE)
#define ALLOWCOMPILE 1
#endif

#if !defined(PACKPATH)
#define PACKPATH 1
#endif

/*
 * Macro for use of buffers
 */
#define INVALID    (-1)						/* invalid $-index value */
#define ISVALID(x) ((x)>=0)					/* test if a $-index is valid */
#define $(offset)  (data.data + (offset))	/* base array in data at $-index */

/*
 * Definition of keys
 */
enum key_offsets {
	key_string = 0,		/* $-index of the key-string */
	key_rules = 1,		/* $-index of the first rule */
	key_previous = 2,	/* $-index of the previous key */
	key_size = 3		/* size of the keys */
};

/*
 * Definition of the dirs
 */
enum dir_offsets {
	dir_string = 0,     /* $-index of the dir-string */
	dir_parent = 1,     /* $-index of the parent dir */
	dir_child = 2,      /* $-index of the first child dir if any */
	dir_depth = 3,      /* depth in the tree */
	dir_next = 4,       /* $-index of the next child dir within the parent */
	dir_action = 5,     /* state for context (see enum actions) */
	dir_size = 6		/* size of the dirs */
};

/*
 * Definition of the rules
 */
enum rule_offsets {
	rule_dir = 0,		/* $-index of the dir */
	rule_kind = 1,		/* kind of the rule (see enum mkinds) */
	rule_next = 2,		/* $-index of the next rule of the key */
	rule_size = 3		/* size of the rules */
};

/*
 * Definitions of compared positions of two directories
 */
enum positions {
	unrelated = 0,	/* dirs are different and no one is parent of the other */
	parent = 1,		/* one dir is parent of the other */
	child = 2, 		/* one dir is child of the other */
	equals = 3		/* the dirs are equals */
};

/*
 * Definition of the kind of the rule
 * The values have to be in numerical order in a such way that
 * most restrictive permissions have lower values.
 */
enum mkinds {
	none = 0,			/* remove the directory content */
	read_only = 1,		/* set read only access to the directory */
	read_write = 2		/* set read write access to the directory */
};

/*
 * Defintion of the actions to perform to the
 * directory tree deduced from applying rules
 *
 * CAUTION! THE ALGORITHM (see function prepare_dir) EXPECT THAT:
 *    mount_none       == mount_none + none
 *    mount_read_only  == mount_none + read_only
 *    mount_read_write == mount_none + read_write
 */
enum actions {
	nothing = 0,		/* no action */
	should_exist,		/* should exist */
	mount_none,			/* content removed */
	mount_read_only,	/* content set read-only */
	mount_read_write	/* content set read/write */
};

/* buffer for all data */
static struct buffer data = { 0, 0, 0 };

/* $-index of the last defined key */
static int last_key = INVALID;

/* $-index of the root directory */
static int root_dir = INVALID;

/* current substitutions */
static const char const *(*substitutions)[2] = 0;

/* current substitutions count */
static int substitutions_count = 0;

/* flag indicating if unshared is made */
static int unshared = 0;

/* scratch mount point */
static const char *scratch_mount_point = "/tmp";

/* the mode for creating directories (assume that umask is set) */
#define MKDIR_MODE   0755

/* length of the mount dir path */
#define MOUNT_PATH_MAX   PATH_MAX

/*
 * Definition of the mountdirs structure used for applying rules recusivly
 */
struct mountdirs {
	struct {
		int length;		/* length of the path */
		char *path;		/* the path points to a string beeing a "char[MOUNT_PATH_MAX]" array */
	}
		origin, 		/* the data for the origin path */
		scratch;		/* the data for the scratch path */
};

/* a needed predeclaration */
static int apply_sub_mount(int diri, enum actions action, const struct mountdirs *mdirs);

#if ALLOWCOMPILE
/*
 * Definitions for compiled binary file
 */
enum file_header {
	header_magic_tag_1   = 0,          /* index of magic tag 1 */
	header_magic_tag_2   = 1,          /* index of magic tag 2 */
	header_magic_version = 2,          /* index of magic version */
	header_data_count    = 3,          /* index of count of data */
	header_last_key      = 4,          /* index of last key $-index */
	header_root_dir      = 5,          /* index of root dir $-index */
	header_size          = 6,          /* size of the header */
	HEADER_MAGIC_TAG_1   = 0x75616d73, /* 'smau' */
	HEADER_MAGIC_TAG_2   = 0x2e68636e, /* 'nch.' */
	HEADER_MAGIC_VERSION = 0x0a312e66  /* 'f.1\n' */
};
#endif

/*
 * Clears the database
 */
static void clear_all()
{
	last_key = INVALID;
	root_dir = INVALID;
	buffer_reinit(&data);
}

/*
 * Get the $-index of the key of name 'key'.
 * If the 'key' doesn't exist, it is created
 * if 'create' isn't nul.
 *
 * Requires: key != NULL
 *
 * Returns the $-index (>= 0) of the 'key' in
 * case of success or if it failed an negative error code value.
 * Possible errors:
 *   -ENOMEM      memory depletion
 *   -ENOENT      not found (only if create == 0)
 */
static int get_key(const char *key, int create)
{
	int result, istr;

	/* search the key */
	result = last_key;
	while (ISVALID(result)) {
		istr = $(result)[key_string];
		if (0 == strcmp(key, (char*)($(istr))))
			return result; /* key found */
		result = $(result)[key_previous];
	}

	/* fail if create not set */
	if (!create)
		return -ENOENT;

	/* allocates the new key and its string */
	result = buffer_alloc(&data, key_size);
	if (result < 0)
		return result;
	istr = buffer_strdup(&data, key);
	if (istr < 0)
		return istr;

	/* init the new key */
	$(result)[key_string] = istr;
	$(result)[key_rules] = INVALID;
	$(result)[key_previous] = last_key;
	last_key = result;

	return result;
}

/*
 * Get the $-index of the dir of 'nparts' subparts 'parts'.
 * If the dir doesn't exist, it is created if 'create' isn't nul.
 * For example, for creating the directory "/foo/bar"
 * you call get_dir({ "foo", "bar" }, 2, 1).
 *
 * Requires: nparts >= 0
 *           && (nparts == 0 || parts != NULL)
 *
 * Returns the $-index (>= 0) of the dir in
 * case of success or if it failed an negative error code value.
 * Possible errors:
 *   -ENOMEM      memory depletion
 *   -ENOENT      not found (only if create == 0)
 */
static int get_dir(const char **parts, int nparts, int create)
{
	int result, parent, depth, sts, istr, found;
	const char *name;

	/* checks */
	assert(nparts >= 0);
	assert(nparts == 0 || parts != NULL);

	/* creation of the root directory if needed */
	if (!ISVALID(root_dir)) {

		/* fail if create not set */
		if (!create)
			return -ENOENT;

		/* allocates the root directory */
		sts = buffer_alloc(&data, dir_size);
		if (sts < 0)
			return sts;

		/* init the root directory */
		root_dir = sts;
		$(sts)[dir_string] = INVALID;
		$(sts)[dir_parent] = INVALID;
		$(sts)[dir_child] = INVALID;
		$(sts)[dir_depth] = 0;
		$(sts)[dir_next] = INVALID;
		$(sts)[dir_action] = nothing;
	}

	/* search the directory, starting from root */
	depth = 0;
	result = root_dir;
	while (depth < nparts) {
		assert(ISVALID(result));
		assert($(result)[dir_depth] == depth);

		/* search the subpart within the current dir */
		name = parts[depth++];
		parent = result;
		result = $(parent)[dir_child];
		found = 0;
		while (!found) {
			if (ISVALID(result)) {
				/* valid entry, test if it is the one searched? */
				istr = $(result)[dir_string];
				if (0 == strcmp(name, (char*)($(istr)))) {
					/* yes, go deepest */
					found = 1;
				} else {
					/* no, iterate on next entries */
					result = $(result)[dir_next];
				}
			} else if (!create) {
				/* fail if create not set */
				return -ENOENT;
			} else {
				/* allocates the directory and its name */
				result = buffer_alloc(&data, dir_size);
				if (result < 0)
					return result;
				istr = buffer_strdup(&data, name);
				if (istr < 0)
					return istr;

				/* init the directory */
				$(result)[dir_string] = istr;
				$(result)[dir_parent] = parent;
				$(result)[dir_child] = INVALID;
				$(result)[dir_depth] = depth;
				$(result)[dir_next] = $(parent)[dir_child];
				$(result)[dir_action] = nothing;
				$(parent)[dir_child] = result;
				found = 1;
			}
		}
		assert(ISVALID(result));
		assert($(result)[dir_depth] == depth);
		assert($(result)[dir_parent] == parent);
		assert(0 == strcmp(name, (char*)($($(result)[dir_string]))));
	}
	assert(ISVALID(result));
	assert($(result)[dir_depth] == nparts);
	return result;
}

#if PACKPATH
/*
 * Packs the 'nparts' of 'parts'. It means that it
 * removes parts "", ".", ".." in the usual way.
 *
 * Returns the count of parts resulting.
 */
static int pack_path(const char **parts, int nparts)
{
	int read, write;

	read = 0;
	write = 0;
	while (read < nparts) {
		switch (parts[read][0]) {
		case 0:
			/* empty part, remove it */
			break;
		case '.':
			if (!parts[read][1]) {
				/* ".", remove it */
				break;
			} else if (parts[read][1] == '.' && !parts[read][2]) {
				/* "..", try to go to the parent except at root */
				if (write)
					write--;
				break;
			}
		default:
			parts[write++] = parts[read];
			break;
		}
		read++;
	}

	return write;
}
#endif

/*
 * Split the 'path' into its 'parts' where 'npartsmax' is the maximum
 * possible parts. There is no copy what means the the string 'path'
 * is used to store the parts values. Pointers of 'parts' are pointing
 * to sub parts of 'path'.
 *
 * CAUTION: 'path' is modified in place and must remain unchanged
 * while the returned 'parts' content is used.
 *
 * Requires: path != NULL
 *           && parts != NULL
 *           && npartsmax >= 0
 *
 * Returns a positive or nul value in case of success indicating the 
 * count of parts computed into 'parts'.
 * In case of failure it returns the negative value -E2BIG indicating
 * that the count of parts computed will exceed 'npartsmax'.
 */
static int split_path(char *path, const char **parts, int npartsmax)
{
	char *iter;
	int nparts;

	/* checks */
	assert(path);
	assert(parts);
	assert(npartsmax >= 0);

	nparts = 0;
	iter = path;
	while (*iter) {
		/* skip any starting / */
		while(*iter && *iter=='/') iter++;
		if (*iter) {
			/* check count */
			if (nparts == npartsmax)
				return -E2BIG;
			/* records the pointer */
			parts[nparts++] = iter++;
			/* searchs the end of the part */
			while(*iter && *iter!='/') iter++;
			/* set the terminating nul if needed */
			if (*iter)
				*iter++ = 0;
		}
	}

#if PACKPATH
	return pack_path(parts, nparts);
#else
	return nparts;
#endif
}

/*
 * Compare the position of the dirs of $-index 'dir1' and 'dir2'.
 *
 * Requires: ISVALID(dir1)
 *           && ISVALID(dir2)
 *
 * Returns:
 *    equals      if 'dir1' == 'dir2'
 *    parent      if 'dir1' is parent of 'dir2'
 *    child       if 'dir1' is child of 'dir2'
 *    unrelated   otherwise
 */
static enum positions cmpdir(int dir1, int dir2)
{
	int d1, d2;

	/* checks */
	assert(ISVALID(dir1));
	assert(ISVALID(dir2));

	/* equallity? */
	if (dir1 == dir2)
		return equals;

	/* get depths */
	d1 = $(dir1)[dir_depth];
	d2 = $(dir2)[dir_depth];
	if (d1 > d2) {
		/* check if the (d1-d2)th parent of dir1 is dir2 */
		while (d1 > d2) {
			dir1 = $(dir1)[dir_parent];
			d1--;
		}
		if (dir1 == dir2)
			return child;
	} else {
		/* check if the (d2-d1)th parent of dir2 is dir1 */
		while (d1 < d2) {
			dir2 = $(dir2)[dir_parent];
			d2--;
		}
		if (dir1 == dir2)
			return parent;
	}
	/* otherwise */
	return unrelated;
}

/*
 * Adds to the key of $-index 'keyi' the rule stating
 * that the dir of $-index 'diri' should bo of 'kind'.
 *
 * Requires: ISVALID(keyi)
 *           && ISVALID(diri)
 *
 * Returns 0 in case of success or a negative error code
 * on failure. The only possible error code is -ENOMEM.
 */
static int add_rule(int keyi, int diri, enum mkinds kind)
{
	int iprvi, nrulei, rulei, stop, waschild;

	/* checks */
	assert(ISVALID(keyi));
	assert(ISVALID(diri));

	/* search insertion point */
	/* the rule is inserted before any child rule */
	/* and after and parent rule */
	iprvi = keyi + key_rules;
	waschild = 0;
	stop = 0;
	while (!stop) {
		/* get the next rule $index */
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
				/* a rule was already set! strange! */
				/* lowering the behaviour is the simplest processing way */
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

	/* allocates the rule */
	rulei = buffer_alloc(&data, rule_size);
	if (rulei < 0)
		return rulei;

	/* init the rule */
	$(rulei)[rule_dir] = diri;
	$(rulei)[rule_kind] = kind;
	$(rulei)[rule_next] = nrulei;
	*$(iprvi) = rulei;

	return 0;
}

/*
 * Recursively set the 'action' to children subdirs of the dir of $-index 'diri'.
 *
 * Requires: ISVALID(diri)
 */
static void set_dir_childs_nothing(int diri)
{
	int child;

	assert(ISVALID(diri));

	child = $(diri)[dir_child];
	while(ISVALID(child)) {
		$(child)[dir_action] = nothing;
		set_dir_childs_nothing(child);
		child = $(child)[dir_next];
	}
}

/*
 * Prepare the context such that the dir of $-index 'diri' will be of 'kind'.
 *
 * Requires: ISVALID(diri)
 */
static void prepare_dir(int diri, enum mkinds kind)
{
	enum actions action;

	assert(ISVALID(diri));

#if !defined(NDEBUG)
	switch(kind) {
	case none: action = mount_none; break;
	case read_only: action = mount_read_only; break;
	case read_write: action = mount_read_write; break;
	default: assert(0); break;
	}
	assert(action == mount_none + kind);
#else
	action = mount_none + kind;
#endif
	if (action > $(diri)[dir_action]) {

		/* wider privilege */
		$(diri)[dir_action] = action;

		/* reset children */
		set_dir_childs_nothing(diri);

		/* force existing path */
		diri = $(diri)[dir_parent];
		while (ISVALID(diri) && $(diri)[dir_action] == nothing) {
			$(diri)[dir_action] = should_exist;
			diri = $(diri)[dir_parent];
		}
	}
}

/*
 * Prepare the context according to the rules of the key of $-index 'keyi'.
 *
 * Requires: ISVALID(keyi)
 */
static void prepare_key(int keyi)
{
	int rulei;

	assert(ISVALID(keyi));

	rulei = $(keyi)[key_rules];
	while (ISVALID(rulei)) {
		prepare_dir($(rulei)[rule_dir], $(rulei)[rule_kind]);
		rulei = $(rulei)[rule_next];
	}
}

/*
 * Get the string value of the string of $-index 'stri'.
 * If a substitution applies to the value, the substituted
 * string is returned in place of the string.
 *
 * Requires: ISVALID(stri)
 *
 * Returns a pointer to the string or the substituted string
 * in case of substition.
 */
static const char *get_string_subst(int stri)
{
	int i;
	const char *result;

	assert(ISVALID(stri));

	result = (const char *)$(stri);
	if (result[0] == '%') {
		i = substitutions_count;
		while (i)
			if (0 == strcmp(result, substitutions[--i][0]))
				return substitutions[i][1];
	}
	return result;
}

/*
 * Computes (recursively) the path of the directory of $-index 'diri'
 * into the 'buffer' of 'length'. 
 *
 * CAUTION:
 *  - No terminating nul will be added at the end of the computed path.
 *  - The root dir case for 'diri' is not well handled
 *  - DON'T CALL THAT FUNCTION USE INSTEED get_dir_path
 *
 * Applies the current substitutions.
 *
 * Requires: ISVALID(diri)
 *           && diri != root_dir
 *           && (buffer || length <= 0)
 * 
 * Returns the positive or null number indicating the count of characters
 * written to 'buffer' or a negative number (-ENAMETOOLONG) if the resulting
 * length is greater than 'length'.
 */
static int get_dir_path_rec(int diri, char *buffer, int length)
{
	const char *name;
	int i, pari, result;

	/* checks */
	assert(ISVALID(diri));
	assert(diri != root_dir);
	assert(buffer || length <= 0);

	/* compute the parent */
	assert(ISVALID($(diri)[dir_parent]));
	pari = $(diri)[dir_parent];
	if (pari == root_dir)
		result = 0;
	else {
		result = get_dir_path_rec(pari, buffer, length);
		if (result < 0)
			return result;
	}

	/* get the name substituted if needed */
	assert(ISVALID($(diri)[dir_string]));
	name = get_string_subst($(diri)[dir_string]);

	/* write now */
	if (result >= length)
		return -ENAMETOOLONG;
	buffer[result++] = '/';
	for (i = 0 ; name[i] ; i++) {
		if (result >= length)
			return -ENAMETOOLONG;
		buffer[result++] = name[i];
	}

	return result;
}

/*
 * Computes the path of the directory of $-index 'diri'
 * into the 'buffer' of 'length'. The computed path will be 
 * terminated with a zero.
 *
 * Applies the current substitutions.
 *
 * Requires: ISVALID(diri)
 *           && (buffer || length <= 0)
 * 
 * Returns the positive or null number indicating the count of characters
 * written to 'buffer' (excluding the terminating nul) or a negative number 
 * (-ENAMETOOLONG) if the resulting length is greater than 'length'.
 */
static int get_dir_path(int diri, char *buffer, int length)
{
	int len;

	/* checks */
	assert(ISVALID(diri));
	assert(buffer || length <= 0);

	if (diri == root_dir) {
		/* case of the root dir */
		if (length < 2)
			return -ENAMETOOLONG;
		buffer[0] = '/';
		buffer[1] = 0;
		len = 1;
	} else {
		/* other cases */
		len = get_dir_path_rec(diri, buffer, length-1);
		if (len < 0)
			return len;
		assert(len < length);
		buffer[len] = 0;
	}

	return len;
}

/*
 * Applies the rules to the subdirectories of the dir of $-index 'diri'.
 * This function is called into a scratch started hierarchy where 'refact'
 * is the action of reference from an already scratch mounted parent.
 * 'mdirs' are the current origin and scratch directories for 'diri'.
 *
 * Requires: ISVALID(diri)
 *           && mdirs != NULL
 *           && refact != nothing
 *           && refact != should_exist
 *
 * Returns 0 in case of success or a negative error code if failed.
 */
static int apply_sub_dirs(int diri, enum actions refact, const struct mountdirs *mdirs)
{
	struct mountdirs mountdirs;
	int result, action, length;
	const char *string;
	char *borg, *bscr;

	/* checks */
	assert(ISVALID(diri));
	assert(mdirs);
	assert(refact != nothing);
	assert(refact != should_exist);

	/* get the first child dir */
	diri = $(diri)[dir_child];
	if (ISVALID(diri)) {

		/* init iteration data */
		mountdirs.origin.path = mdirs->origin.path;
		borg = &mountdirs.origin.path[mdirs->origin.length];
		mountdirs.scratch.path = mdirs->scratch.path;
		bscr = &mountdirs.scratch.path[mdirs->scratch.length];
		*borg++ = '/';
		*bscr++ = '/';

		/* iterate on children dirs */
		do {
			/* get action to perform */
			action = $(diri)[dir_action];
			if (action != nothing) {

				/* get the dir name and its length (with '/' ahead counted) */
				assert(ISVALID($(diri)[dir_string]));
				string = get_string_subst($(diri)[dir_string]);
				length = (int)strlen(string) + 1;

				/* set the resulting length and check it */
				mountdirs.origin.length = mdirs->origin.length + length;
				mountdirs.scratch.length = mdirs->scratch.length + length;
				if (mountdirs.origin.length >= MOUNT_PATH_MAX || mountdirs.scratch.length >= MOUNT_PATH_MAX)
					return -ENAMETOOLONG;

				/* make the paths (length includes the terminating zero) */
				memcpy(borg, string, length);
				memcpy(bscr, string, length);

				/* ensure existing scratch dir */
				/* this way of processing avoid the stat system call but doesn't ensure that scratch is a directory */
				result = mkdir(mountdirs.scratch.path, MKDIR_MODE);
				if (result < 0 && errno != EEXIST)
					return -errno;

				if (action == should_exist || action == refact) {
					/* nothing special to mount, apply to sub dirs */
					result = apply_sub_dirs(diri, refact, &mountdirs);
				} else {
					/* mount now */
					result = apply_sub_mount(diri, action, &mountdirs);
				}
				if (result)
					return result;
			}
			/* next */
			diri = $(diri)[dir_next];
		} while (ISVALID(diri));

		/* restore previous mount paths */
		*--borg = 0;
		*--bscr = 0;
	}
	return 0;
}

/*
 * Mounts the dir of $-index 'diri' according to the rules of 'action'
 * and apply the rules to its sub directories.
 * This function is called into a scratch started hierarchy.
 * 'mdirs' are the current origin and scratch directories for 'diri'.
 *
 * Note: 'action' is the same that $(diri)[dir_action] but is given as
 * parameter because is available by the caller.
 *
 * Requires: ISVALID(diri)
 *           && mdirs != NULL
 *           && action == $(diri)[dir_action]
 *
 * Returns 0 in case of success or a negative error code if failed.
 */
static int apply_sub_mount(int diri, enum actions action, const struct mountdirs *mdirs)
{
	int result;
	unsigned long bind;

	/* checks */
	assert(ISVALID(diri));
	assert(mdirs);
	assert(action == $(diri)[dir_action]);

	/* set the bind flag */
	bind = action != mount_none ? MS_BIND : 0;

	/* apply the mount */
	result = mount(mdirs->origin.path, mdirs->scratch.path, "ramfs", bind, 0);
	if (result < 0)
		return -errno;

	/* apply children mounts */
	result = apply_sub_dirs(diri, action, mdirs);
	if (result)
		return result;

	/* remount read only if needed */
	if (action != mount_read_write) {
		result = mount("", mdirs->scratch.path, "", bind|MS_REMOUNT|MS_RDONLY, 0);
		if (result < 0)
			return -errno;
	}

	return 0;
}

/*
 * Mounts the dir of $-index 'diri' according to the rules of 'action'
 * and apply the rules to its sub directories.
 * This function starts the scratch hierarchy and at end move it 
 * to its destination.
 *
 * Requires: ISVALID(diri)
 *           && action == $(diri)[dir_action]
 *
 * Returns 0 in case of success or a negative error code if failed.
 */
static int apply_main_mount(int diri)
{
	char scratch[MOUNT_PATH_MAX], origin[MOUNT_PATH_MAX];
	struct mountdirs mountdirs;
	int result;

	/* checks */
	assert(ISVALID(diri));

	/* lazy unsharing namespace on need */
	if (!unshared) {
		/* unshare */
		result = unshare(CLONE_NEWNS);
		if (result < 0)
			return -errno;
		/* remount all recursively as slave */
		result = mount("", "/", "", MS_SLAVE|MS_REC, 0);
		if (result < 0)
			return -errno;
		/* set unshared */
		unshared = 1;
	}

	/* get the reference filesystem mount point */
	result = get_dir_path(diri, origin, sizeof origin);
	if (result < 0)
		return result;

	mountdirs.origin.length = result;
	mountdirs.origin.path = origin;

	/* get the scratch mount point */
	result = strlen(scratch_mount_point);
	assert(MOUNT_PATH_MAX > result);
	memcpy(scratch, scratch_mount_point, result + 1);

	mountdirs.scratch.length = result;
	mountdirs.scratch.path = scratch;

	/* mount the directory now */
	result = apply_sub_mount(diri, $(diri)[dir_action], &mountdirs);
	if (result)
		return result;

	/* move the hierachy now */
	result = mount(scratch, origin, "", MS_MOVE, 0);
	return result;
}

/*
 * Apply the rules to the sub directories of the dir of $-index 'diri'.
 *
 * Requires: ISVALID(diri)
 *
 * Returns 0 in case of success or a negative error code if failed.
 */
static int apply_main_dirs(int diri)
{
	int result;

	/* checks */
	assert(ISVALID(diri));

	/* get the first child and iterate on children dirs */
	diri = $(diri)[dir_child];
	while (ISVALID(diri)) {
		switch ($(diri)[dir_action]) {

		case nothing:
			/* out of mounting scope dir, dont explore children */
			break;

		case mount_read_write:
			/* read-write should be the default, process as if should-exist */

		case should_exist:
			/* if it should exist then it is required by some remounted subdir */
			result = apply_main_dirs(diri);
			if (result)
				return result;
			break;

		default:
			/* do the mounting of the dir and its sub dirs */
			result = apply_main_mount(diri);
			if (result)
				return result;
			break;
		}
		/* next child */
		diri = $(diri)[dir_next];
	}
	return 0;
}

/*
 * Reads the textual database of 'file'.
 *
 * CAUTION, in case of error the returned data state is undefined.
 *
 * Requires: file >= 0
 *           && !ISVALID(last_item)
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_textual_database(int file)
{
	struct parse parse;
	int sts, keyi, diri;
	enum mkinds kind;
	const char *parts[64];

	/* checks */
	assert(file >= 0);
	assert(!ISVALID(last_key));

	/* init the parse */
	parse_init(&parse, file);

	/* parse the file */
	keyi = INVALID;
	sts = 0;
	while(!parse.finished && !sts) {

		/* read one line */
		sts = parse_line(&parse);
		if (sts) {
			if (parse_is_syntax_error(sts))
				sts = parse_make_syntax_error(
						parse_syntax_error_number(sts) - parse_line_too_long + fs_line_too_long,
						parse_syntax_error_line(sts));
		} else {
			switch (parse.fieldcount) {
			case 0:
				assert(parse.finished);
				break;

			case 1: /* defines a key */
				if (parse.begsp) {
					sts = parse_make_syntax_error(fs_directory_incomplete, parse.lino);
				} else {
					keyi = get_key(parse.fields[0], 1);
					if (!ISVALID(keyi)) 
						sts = keyi;
				}
				break;

			case 2: /* defines a directory */
				if (!parse.begsp) {
					sts = parse_make_syntax_error(fs_directory_incomplete, parse.lino);
				} else if (!ISVALID(keyi)) {
					sts = parse_make_syntax_error(fs_no_key_set, parse.lino);
				} else if (parse.fields[1][0] != '/') {
					sts = parse_make_syntax_error(fs_bad_directory, parse.lino);
				} else {
					if (0 == strcmp(parse.fields[0], "-")) {
						kind = none;
					} else if (0 == strcmp(parse.fields[0], "+r")) {
						kind = read_only;
					} else if (0 == strcmp(parse.fields[0], "+rw")) {
						kind = read_write;
					} else {
						sts = parse_make_syntax_error(fs_wrong_permission, parse.lino);
					}
					if (!sts) {
						sts = split_path(parse.fields[1], parts, sizeof parts / sizeof * parts);
						if (!sts) {
							sts = parse_make_syntax_error(fs_root_directory, parse.lino);
						} else if (sts < 0) {
							sts = parse_make_syntax_error(fs_bad_directory_depth, parse.lino);
						} else {
							diri = get_dir(parts, sts, 1);
							if (!ISVALID(diri)) 
								sts = diri;
							else
								sts = add_rule(keyi, diri, kind);
						}
					}
				}
				break;

			default:
				sts = parse_make_syntax_error(fs_too_many_fields, parse.lino);
				break;
			}
		}
	}
	/* is empty ? */
	if (!sts && !ISVALID(last_key))
		sts = parse_make_syntax_error(fs_file_empty, parse.lino);

	return sts;
}

#if ALLOWCOMPILE
/*
 * Saves the database to the binary compiled database 
 * of 'path' filename.
 *
 * Requires: path != NULL
 *           && ISVALID(last_key)
 *
 * Returns 0 in case of success or a negative error code otherwise.
 */
static int save_compiled_database(const char *path)
{
	int file, result, head[header_size];
	struct iovec iovect[2];
	ssize_t writen;

	/* checks */
	assert(path);
	assert(ISVALID(last_key));

	/* init the header data */
	head[header_magic_tag_1] = HEADER_MAGIC_TAG_1;
	head[header_magic_tag_2] = HEADER_MAGIC_TAG_2;
	head[header_magic_version] = HEADER_MAGIC_VERSION;
	head[header_data_count] = data.count;
	head[header_last_key] = last_key;
	head[header_root_dir] = root_dir;

	/* init the iovect data */
	iovect[0].iov_base = head;
	iovect[0].iov_len = sizeof head;

	iovect[1].iov_base = data.data;
	iovect[1].iov_len = data.count * sizeof * data.data;

	/* open the file for write */
	file = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	if (file < 0)
		return file;

	/* write it now */
	writen = writev(file, iovect, 2);
	result = writen < 0 ? -errno : 0;
	close(file);
	return result;
}

/*
 * Reads the binary compiled database of 'file'
 *
 * CAUTION, in case of error the returned data state is undefined.
 *
 * Requires: file >= 0
 *           && !ISVALID(last_key)
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_compiled_database(int file)
{
	int head[header_size];
	ssize_t readen;

	/* checks */
	assert(file >= 0);
	assert(!ISVALID(last_key));

	/* read the header */
	readen = read(file, head, sizeof head);
	if (readen < 0) 
		return -errno;
	if (readen < sizeof head)
		return -ENOEXEC;

	/* check magic data */
	if (head[header_magic_tag_1] != HEADER_MAGIC_TAG_1
		|| head[header_magic_tag_2] != HEADER_MAGIC_TAG_2
		|| head[header_magic_version] != HEADER_MAGIC_VERSION)
		return -ENOEXEC;

	/* allocate memory */
	data.data = malloc(head[header_data_count] * sizeof * data.data);
	if (!data.data)
		return -ENOMEM;

	/* init data */
	data.count = head[header_data_count];
	data.capacity = head[header_data_count];
	last_key = head[header_last_key];
	root_dir = head[header_root_dir];

	/* read all now */
	readen = read(file, data.data, data.count * sizeof * data.data);
	return readen < 0 ? -errno : 0;
}
#endif

/*
 * Reads the database of 'file'
 *
 * Requires: file >= 0
 *           && !ISVALID(last_key)
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_database(int file)
{
	int result;
	off_t pos;

	assert(file >= 0);
	assert(!ISVALID(last_key));

	pos = lseek(file, 0, SEEK_CUR);
#if ALLOWCOMPILE
	result = read_compiled_database(file);
	if (result == -ENOEXEC && pos != ((off_t)-1)) {
		if (pos == lseek(file, pos, SEEK_SET))
			result = read_textual_database(file);
		else
			result = -errno;
	}
#else
	result = read_textual_database(file);
#endif

	if (result) {
		clear_all();
		if (pos != ((off_t)-1))
			lseek(file, pos, SEEK_SET);
	}

	assert(result <= 0);
	assert(smaunch_fs_has_database() == !result);

	return result;
}

/* see comment in smaunch-fs.h */
enum smaunch_fs_substitution_check_code smaunch_fs_check_substitution_pair(const char const *pattern, const char const *replacement)
{
	/* checks the pattern */
	if (!pattern)
		return fs_substitution_pattern_is_null;
	if (*pattern++ != '%')
		return fs_substitution_pattern_hasnt_percent;
	if (!*pattern)
		return fs_substitution_pattern_is_percent;
	while (*pattern)
		if (*pattern++ == '/')
			return fs_substitution_pattern_has_slash;

	/* checks the replacement */
	if (!replacement)
		return fs_substitution_replacement_is_null;
	if (!*replacement)
		return fs_substitution_replacement_is_empty;
	while (*replacement)
		if (*replacement++ == '/')
			return fs_substitution_replacement_has_slash;

	/* valid */
	return fs_substitution_is_valid;
}

/* see comment in smaunch-fs.h */
int smaunch_fs_valid_substitutions(const char const *substs[][2], int count)
{
	int i;

	if (count < 0)
		return 0;
	if (count == 0)
		return 1;
	if (!substs)
		return 0;

	for(i = 0 ; i < count ; i++)
		if (smaunch_fs_check_substitution_pair(substs[i][0], substs[i][1]) != fs_substitution_is_valid)
			return 0;

	return 1;
}

/* see comment in smaunch-fs.h */
void smaunch_fs_set_substitutions(const char const *substs[][2], int count)
{
	assert(smaunch_fs_valid_substitutions(substs, count));
	substitutions = substs;
	substitutions_count = count;
}

/* see comment in smaunch-fs.h */
int smaunch_fs_has_database()
{
	return ISVALID(last_key);
}

/* see comment in smaunch-fs.h */
int smaunch_fs_load_database(const char *path)
{
	int result, file;

	assert(path != NULL);

	/* clear all */
	clear_all();

	/* open the file for read */
	file = open(path, O_RDONLY);
	if (file < 0)
		return -errno;

	/* read and close */
	result = read_database(file);
	close(file);

	assert(result <= 0);
	assert(smaunch_fs_has_database() == !result);

	return result;
}

/* see comment in smaunch-fs.h */
int smaunch_fs_save_database_compiled(const char *path)
{
	assert(path);
	assert(smaunch_fs_has_database());

#if ALLOWCOMPILE
	return save_compiled_database(path);
#else
	return -ENOTSUP;
#endif
}

/* see comment in smaunch-fs.h */
int smaunch_fs_has_key(const char *key)
{
	assert(key);
	assert(smaunch_fs_has_database());

	return ISVALID(get_key(key, 0));
}

/* see comment in smaunch-fs.h */
void smaunch_fs_context_start()
{
	assert(smaunch_fs_has_database());

	if (ISVALID(root_dir)) {
		$(root_dir)[dir_action] = nothing;
		set_dir_childs_nothing(root_dir);
	}
}

/* see comment in smaunch-fs.h */
int smaunch_fs_context_add(const char *key)
{
	int keyi;

	assert(key);
	assert(smaunch_fs_has_database());

	keyi = get_key(key, 0);
	if (keyi < 0)
		return keyi;

	prepare_key(keyi);

	return 0;
}

/* see comment in smaunch-fs.h */
int smaunch_fs_context_apply()
{
	assert(smaunch_fs_has_database());

	unshared = 0;
	return apply_main_dirs(root_dir);
}




#ifdef TEST_FS
#include <stdio.h>

const char *dir2str(int diri)
{
	static char tampon[PATH_MAX];
	get_dir_path(diri, tampon, (int)(sizeof tampon));
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
		dump_rules($(keyi)[key_rules]);
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
		smaunch_fs_context_start();
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
	n = smaunch_fs_load_database("db.fs");
	printf("\nLOADING dbfs: %d\n", n);
	if (!n) {
		dump_all();
		apply_all();
	}
	
	return 0;
}
#endif
