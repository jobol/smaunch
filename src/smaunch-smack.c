/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
/*
 * NOTE: This version is not final
 *  - improve compiled format (remove unused data, check magic and version)
 *  - mmap compiled
 *  - comment the code
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <sys/uio.h>

#include "smaunch-smack.h"
#include "smack-utils-label.h"
#include "smack-utils-coda.h"
#include "smack-utils-fs.h"
#include "buffer.h"
#include "parse.h"

/* for simulation usage */
#if !defined(SIMULATION)
#define SIMULATION 0
#endif

/* for compilation of databases */
#if !defined(ALLOWCOMPILE)
#define ALLOWCOMPILE 1
#endif

/*
 * Macro for use of buffers
 */
#define INVALID    (-1)								/* invalid $-index value */
#define ISVALID(x) ((x)>=0)							/* test if a $-index is valid */
#define $(offset)  (data_buffer.data + (offset))	/* base array in data at $-index */
#define H          hash_buffer.data					/* hash array */

/*
 * Definition of items
 * Items are string data referencing either keys or objects.
 */
enum item_offsets {
	item_hash = 0,		/* hash code value of the string */
	item_string = 1,	/* $-index of the string */
	item_rules = 2,		/* $-index of the first rule */
	item_object = 3,	/* index of the object */
	item_previous = 4,	/* $-index of the previous item */
	item_size = 5		/* size of the items */
};

/*
 * Definition of the rules
 */
enum rule_offsets {
	rule_object = 0,	/* index of the object */
	rule_access = 1,	/* smack coda for the permission of the rule */
	rule_previous = 2,	/* $-index of the previous rule in the key */
	rule_size = 3		/* size of the rules */
};


/* count of items */
static int item_count = 0;

/* $-index of the head of the list of items */
static int last_item = INVALID;

/* buffer for the hash table */
static struct buffer hash_buffer = { 0, 0, 0 };

/* buffer for the items, the strings and the rules */
static struct buffer data_buffer = { 0, 0, 0 };

/* count of objects managed */
static int object_count = 0;

/* $-index of the object's string names */
static int *object_strings = 0;

/* smack permissions of reference */
static smack_coda *coda_reference = 0;

/* smack permissions from the active context */
static smack_coda *coda_context = 0;

/* current subject of the rules */
static const char *current_subject = "User";

#if ALLOWCOMPILE
/*
 * Definitions for compiled binary file
 */
enum file_header {
	header_magic_tag_1   = 0,          /* index of magic tag 1 */
	header_magic_tag_2   = 1,          /* index of magic tag 2 */
	header_magic_version = 2,          /* index of magic version */
	header_data_count    = 3,          /* index of count of data */
	header_hash_count    = 4,          /* index of count of hash */
	header_item_count    = 5,          /* index of count of items */
	header_object_count  = 6,          /* index of count of objects */
	header_last_item     = 7,          /* index of last item $-index */
	header_size          = 8,          /* size of the header */
	HEADER_MAGIC_TAG_1   = 0x75616d73, /* 'smau' */
	HEADER_MAGIC_TAG_2   = 0x2e68636e, /* 'nch.' */
	HEADER_MAGIC_VERSION = 0x0a312e73  /* 's.1\n' */
};
#endif

#ifdef TEST
void cdump(const char *name, smack_coda *context);
void dump();
#endif

/*
 * Clears the database
 */
static void clear_database()
{
	buffer_reinit(&hash_buffer);
	buffer_reinit(&data_buffer);
	free(coda_reference);
	free(coda_context);
	free(object_strings);
	object_count = 0;
	item_count = 0;
	last_item = INVALID;
	coda_reference = 0;
	coda_context = 0;
	object_strings = 0;
}

/*
 * Get the $-index of the item of key 'text'.
 * If the item of key 'text' doesn't exist, it is created
 * if 'create' isn't nul.
 *
 * Requires: text != NULL
 *
 * Returns the $-index (>= 0) of the item of key 'text' in
 * case of success or if it failed an negative error code value.
 * Possible errors:
 *   -ENOMEM      memory depletion
 *   -ENOENT      not found (only if create == 0)
 */
static int item_get(const char *text, int create)
{
	int hash, len, iter, indic, istr, i, result;

	/* compute the hash code and the string length */
	for (len = 0, hash = 5381 ; text[len] ; len++)
		hash = hash * 33 + (unsigned)text[len];
	hash &= INT_MAX;

	/* search the item */
	if (hash_buffer.count) {
		/* first item of the item list at hash code */
		iter = hash % hash_buffer.count;
		result = H[iter];
		assert(!ISVALID(result) || (0 <= result && result < data_buffer.count));
		while (ISVALID(result)) {
			/* is the item matching hash and string? */
			if ($(result)[item_hash] == hash) {
				istr = $(result)[item_string];
				if (0 == strcmp(text, (char*)($(istr)))) {
					/* yes, found the item */
					return result;
				}
			}
			/* not found, try the next item of the list */
			iter = (iter ? iter : hash_buffer.count) - 1;
			result = H[iter];
			assert(!ISVALID(result) || (0 <= result && result < data_buffer.count));
		}
	}

	/* fail if create not set */
	if (!create)
		return -ENOENT;

	/* grow if needed */
	if (item_count >= (hash_buffer.count >> 2)) {
		/* extend the hash array */
		i = buffer_set_count(&hash_buffer, hash_buffer.count + 1024);
		if (i < 0)
			return i;

		/* reinit the hash array */
		i = hash_buffer.count;
		while (i)
			H[--i] = INVALID;

		/* records all the items into the hash */
		i = last_item;
		while (ISVALID(i)) {
			/* insert the item of $-index i */
			iter = $(i)[item_hash] % hash_buffer.count;
			indic = H[iter];
			assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
			while (ISVALID(indic)) {
				iter = (iter ? iter : hash_buffer.count) - 1;
				indic = H[iter];
				assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
			}
			H[iter] = i;
			/* next item (the previous) */
			i = $(i)[item_previous];
		}
	}

	/* allocate the new item and its string */
	result = buffer_alloc(&data_buffer, item_size);
	if (result < 0)
		return result;
	istr = buffer_strndup(&data_buffer, text, len);
	if (istr < 0)
		return istr;

	/* init the new item */
	$(result)[item_hash] = hash;
	$(result)[item_string] = istr;
	$(result)[item_rules] = INVALID;
	$(result)[item_object] = INVALID;
	$(result)[item_previous] = last_item;
	last_item = result;
	item_count++;

	/* insert the new item in the hash */
	iter = hash % hash_buffer.count;
	indic = H[iter];
	assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
	while (ISVALID(indic)) {
		iter = (iter ? iter : hash_buffer.count) - 1;
		indic = H[iter];
		assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
	}
	H[iter] = result;

	/* end */
	return result;
}

/*
 * Adds the rule of 'object' and permission 'access' to the key item
 * of $-index 'keyi'.
 *
 * Requires: ISVALID(keyi)
 *           && object != NULL
 *           && access != NULL
 *           && smack_object_is_valid(object)
 *           && smack_coda_string_is_valid(access)
 *           && !ISVALID(last_item)
 *
 * Returns 0 in case of success or a negative error code if failed.
 *
 * Possible errors:
 *   -ENOMEM      memory depletion
 */
static int add_key_rule(int keyi, const char *object, const char *access)
{
	int obji, rulei, objno;

	/* checks */
	assert(ISVALID(keyi));
	assert(object);
	assert(access);
	assert(smack_object_is_valid(object));
	assert(smack_coda_string_is_valid(access));
	assert(!ISVALID(last_item));

	/* get the item for the object */
	obji = item_get(object, 1);
	if (obji < 0)
		return obji;

	/* get the object number */
	objno = $(obji)[item_object];
	if (!ISVALID(objno)) {
		/* create the object if it doesn't exist */
		objno = object_count++;
		$(obji)[item_object] = objno;
	}

	/* allocates data for the rule */
	rulei = buffer_alloc(&data_buffer, rule_size);
	if (rulei < 0)
		return rulei;

	/* init the rule */
	$(rulei)[rule_object] = objno;
	$(rulei)[rule_access] = (int)smack_coda_from_string(access);
	$(rulei)[rule_previous] = $(keyi)[item_rules];
	$(keyi)[item_rules] = rulei;

	return 0;
}

/*
 * Adds the rules attached to the key item of $-index 'keyi'
 * to the permissions set 'codas'.
 *
 * Requires: codas != NULL
 *           && (codas == coda_reference || codas == coda_context)
 *           && ISVALID(keyi)
 *           && ISVALID(last_item)
 */
static void add_context_key(smack_coda *codas, int keyi)
{
	int rulei, obji;
	smack_coda coda;

	/* checks */
	assert(codas);
	assert(codas == coda_reference || codas == coda_context);
	assert(ISVALID(keyi));
	assert(ISVALID(last_item));

	/* iterate on the rules of the key item */
	rulei = $(keyi)[item_rules];
	while (ISVALID(rulei)) {
		obji = $(rulei)[rule_object];
		assert(ISVALID(obji));
		assert(0 <= obji);
		assert(obji < object_count);
		coda = (smack_coda)$(rulei)[rule_access];
		assert(smack_coda_is_valid(coda));
		codas[obji] |= coda; /* or of all permissions */
		rulei = $(rulei)[rule_previous];
	}
}

/*
 * Writes the smack rules corresponding to the permissions set 'codas'
 * to the 'file'.
 *
 * The flags 'multi' and 'diff' are controling the behaviour:
 *
 *  - 'multi': If zeroed, writting the rules will not be buffered and
 *             the rules will be written line by line. If not zeroed
 *             the rules will be buffered and written by blocks.
 *             USING multi!=0 IMPROVES GREETLY THE SPEED.
 *             dont set multi to 0 except for good reasons.
 *
 *  - 'diff': If zeroed, all rules will be written. If not zeroed
 *            only the rules differing from the reference will be written.
 *            USING diff!=1 IMPROVES THE SPEED.
 *
 * Requires: codas != NULL
 *           && (codas == coda_reference || codas == coda_context)
 *           && ISVALID(last_item)
 *
 * Returns 0 on success or a negative error code on error.
 */
static int context_apply(const smack_coda *codas, int file, int multi, int diff)
{
	char buffer[8192], *rule;
	unsigned index, pos, lr, wr, ls, ms;
	int state;
	smack_coda coda;
	ssize_t sts;

	assert(codas);
	assert(codas == coda_reference || codas == coda_context);
	assert(ISVALID(last_item));

	/* compute the subject length */
	ls = (int)strlen(current_subject);

	/* init the loop that iterate on objects */
	pos = 0;
	index = object_count;
	state = 1;
	while(state) {
		switch (state) {
		case 1:
			/* terminated? */
			if (!index) {
				state = 10; /* flush and end */
				break;
			}

			/* exams the new coda */
			coda = codas[--index];
			if (diff && coda == coda_reference[index])
				break; /* no change */

			/* changed, rule to emit */
			rule = (char*)($(object_strings[index]));
			lr = strlen(rule);
			ms = ls + lr + smack_coda_bits_count + 3; /* max size: subject + object + max permissions + 2 spaces + 1 terminating nul */
			assert(ms <= sizeof buffer);

			/* flush if needed */
			if (pos + ms > sizeof buffer) {
				state = 12;
				break;
			}

		case 2:
			/* fill the buffer */
			assert(pos + ms <= sizeof buffer);
			memcpy(buffer + pos, current_subject, ls);
			pos += ls;
			buffer[pos++] = ' ';
			memcpy(buffer + pos, rule, lr);
			pos += lr;
			buffer[pos++] = ' ';
			pos += smack_coda_to_string(coda, buffer + pos, smack_coda_bits_count);
			buffer[pos++] = '\n';
			state = multi ? 1 : 11;
			break;

		default:
			/* flush if needed */
			if (pos) {
				wr = 0;
				while (wr < pos) {
					sts = write(file, buffer+wr, pos-wr);
					if (sts < 0)
						return sts;
					wr += (unsigned)sts;
				}
				pos = 0;
			}
			state -= 10;
			break;
		}
	}

	return 0;
}

/*
 * Reads the database of 'path' filename.
 *
 * CAUTION, in case of error the returned data state is undefined.
 *
 * Requires: path != NULL
 *           && !ISVALID(last_item)
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_database_internal(const char *path)
{
	struct parse parse;
	int sts, keyi, k;

	/* checks */
	assert(path);
	assert(!ISVALID(last_item));

	/* init the parse */
	sts = parse_init_open(&parse, path);
	if (sts < 0)
		return sts;
	assert(sts == 0);

	/* parse the file */
	keyi = INVALID;
	while(!parse.finished && !sts) {

		/* read one line */
		sts = parse_line(&parse);
		if (!sts) {
			switch (parse.fieldcount) {
			case 0: /* nothing */
				assert(parse.finished);
				break;

			case 1: /* key */
				if (parse.begsp)
					/* space ahead */
					sts = parse_make_syntax_error(smack_object_without_access,parse.lino);
				else {
					/* records the key */
					k = item_get(parse.fields[0], 1);
					if (ISVALID(k))
						keyi = k;
					else
						sts = k;
				}
				break;

			case 2: /* rule: object access */
				if (!parse.begsp)
					/* no space ahead */
					sts = parse_make_syntax_error(smack_extra_after_key,parse.lino);

				else if (!ISVALID(keyi))
					/* no current key */
					sts = parse_make_syntax_error(smack_no_key_set, parse.lino);

				else if (!smack_object_is_valid(parse.fields[0]))
					/* invalid smack object label */
					sts = parse_make_syntax_error(smack_invalid_object, parse.lino);

				else if (!smack_coda_string_is_valid(parse.fields[1]))
					/* invalid smack permission */
					sts = parse_make_syntax_error(smack_invalid_access, parse.lino);

				else
					/* check are okay, add the rule */
					sts = add_key_rule(keyi, parse.fields[0], parse.fields[1]);

				break;

			default:
				sts = parse_make_syntax_error(smack_extra_after_access, parse.lino);
			}
		}
	}
	/* close the file */
	close(parse.file);

	/* stop if error */
	assert(sts <= 0);
	if (sts)
		return sts;

	/* error if no key */
	if (!ISVALID(last_item))
		return parse_make_syntax_error(smack_file_empty, parse.lino);

	/* allocate the codas arrays and the strings array of string's $-indexes */
	coda_reference = calloc(object_count, sizeof * coda_reference);
	coda_context = calloc(object_count, sizeof * coda_context);
	object_strings = calloc(object_count, sizeof * object_strings);
	if (!coda_reference || !coda_context || !object_strings)
		return -ENOMEM;

	/* init the coda of reference and the strings */
	keyi = last_item;
	while (ISVALID(keyi)) {
		if (ISVALID($(keyi)[item_rules])) 
			add_context_key(coda_reference, keyi);
		if (ISVALID($(keyi)[item_object])) 
			object_strings[$(keyi)[item_object]] = $(keyi)[item_string];
		keyi = $(keyi)[item_previous];
	}

	assert(ISVALID(last_item));
	return 0;
}

/*
 * Reads the database of 'path' filename.
 *
 * Requires: path != NULL
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_database(const char *path)
{
	int result;

	assert(path);

	clear_database();

	assert(!ISVALID(last_item));

	result = read_database_internal(path);
	if (result)
		clear_database();

	assert(ISVALID(last_item) || result);

	return result;
}

#if ALLOWCOMPILE
/*
 * Saves the database to the binary compiled database 
 * of 'path' filename.
 *
 * Requires: path != NULL
 *           && ISVALID(last_item)
 *
 * Returns 0 in case of success or a negative error code otherwise.
 */
static int save_compiled_database(const char *path)
{
	int file, result, head[header_size];
	struct iovec iovect[5];
	ssize_t writen;

	/* checks */
	assert(path);
	assert(ISVALID(last_item));

	/* init the header data */
	head[header_magic_tag_1] = HEADER_MAGIC_TAG_1;
	head[header_magic_tag_2] = HEADER_MAGIC_TAG_2;
	head[header_magic_version] = HEADER_MAGIC_VERSION;
	head[header_data_count] = data_buffer.count;
	head[header_hash_count] = hash_buffer.count;
	head[header_item_count] = item_count;
	head[header_object_count] = object_count;
	head[header_last_item] = last_item;

	/* init the iovect data */
	iovect[0].iov_base = head;
	iovect[0].iov_len = sizeof head;

	iovect[1].iov_base = data_buffer.data;
	iovect[1].iov_len = data_buffer.count * sizeof * data_buffer.data;

	iovect[2].iov_base = hash_buffer.data;
	iovect[2].iov_len = hash_buffer.count * sizeof * hash_buffer.data;
	
	iovect[3].iov_base = coda_reference;
	iovect[3].iov_len = object_count * sizeof * coda_reference;

	iovect[4].iov_base = object_strings;
	iovect[4].iov_len = object_count * sizeof * object_strings;

	/* open the file for write */
	file = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
	if (file < 0)
		return file;

	/* write it now */
	writen = writev(file, iovect, 5);
	result = writen < 0 ? -errno : 0;
	close(file);
	return result;
}

/*
 * Reads the binary compiled database of 'path' filename.
 *
 * CAUTION, in case of error the returned data state is undefined.
 *
 * Requires: path != NULL
 *           && !ISVALID(last_item)
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_compiled_database_internal(const char *path)
{
	int file, result, head[header_size];
	struct iovec iovect[4];
	ssize_t readen;

	/* checks */
	assert(path);
	assert(!ISVALID(last_item));

	/* open the file for read */
	file = open(path, O_RDONLY);
	if (file < 0)
		return file;

	/* read the header */
	readen = read(file, head, sizeof head);
	if (readen < 0) {
		result = -errno;
		close(file);
		return result;
	}
	if (readen < sizeof head) {
		close(file);
		return -EINTR;
	}

	/* check magic data */
	if (head[header_magic_tag_1] != HEADER_MAGIC_TAG_1
		|| head[header_magic_tag_2] != HEADER_MAGIC_TAG_2
		|| head[header_magic_version] != HEADER_MAGIC_VERSION) {
		close(file);
		return -EBADF;
	}

	/* allocate memory */
	data_buffer.data = malloc(head[header_data_count] * sizeof * data_buffer.data);
	hash_buffer.data = malloc(head[header_hash_count] * sizeof * hash_buffer.data);
	coda_reference = malloc(head[header_object_count] * sizeof * coda_reference);
	coda_context = calloc(head[header_object_count], sizeof * coda_reference);
	object_strings = malloc(head[header_object_count] * sizeof * object_strings);
	if (!data_buffer.data || !hash_buffer.data || !coda_reference || !coda_context) {
		close(file);
		return -ENOMEM;
	}

	/* init data */
	data_buffer.count = head[header_data_count];
	data_buffer.capacity = head[header_data_count];
	hash_buffer.count = head[header_hash_count];
	hash_buffer.capacity = head[header_hash_count];
	item_count = head[header_item_count];
	object_count = head[header_object_count];
	last_item = head[header_last_item];

	/* init the iovect data */
	iovect[0].iov_base = data_buffer.data;
	iovect[0].iov_len = data_buffer.count * sizeof * data_buffer.data;

	iovect[1].iov_base = hash_buffer.data;
	iovect[1].iov_len = hash_buffer.count * sizeof * hash_buffer.data;
	
	iovect[2].iov_base = coda_reference;
	iovect[2].iov_len = object_count * sizeof * coda_reference;

	iovect[3].iov_base = object_strings;
	iovect[3].iov_len = object_count * sizeof * object_strings;

	/* read all now */
	readen = readv(file, iovect, 4);
	result = readen < 0 ? -errno : 0;
	close(file);
	return result;
}

/*
 * Reads the binary compiled database of 'path' filename.
 *
 * Requires: path != NULL
 *
 * Returns 0 in case of success or a negative error code in case of error.
 */
static int read_compiled_database(const char *path)
{
	int result;

	assert(path);

	clear_database();

	assert(!ISVALID(last_item));

	result = read_compiled_database_internal(path);
	if (result)
		clear_database();

	assert(ISVALID(last_item) || result);

	return result;
}
#endif

/* see comment in smaunch-smack.h */
int smaunch_smack_load_database(const char *path)
{
#if ALLOWCOMPILE
	char cpldb[PATH_MAX];
	struct stat sdb, scpl;
	int result;

	assert(path != NULL);

	/* get info about database */
	result = stat(path, &sdb);
	if (result < 0)
		return -errno;

	/* get name of the compiled database */
	result = snprintf(cpldb, sizeof cpldb, "%s.compiled", path);
	if (result >= (int)sizeof cpldb)
		return -ENAMETOOLONG;

	/* test if available */
	result = stat(cpldb, &scpl);
	if (!result && sdb.st_mtime < scpl.st_mtime) {
		/* try to use the compiled database */
		result = read_compiled_database(cpldb);
		if (!result)
			return 0;
	}

	/* compile the database */
	result = read_database(path);
	if (!result)
		save_compiled_database(cpldb);

	return result;
#else
	assert(path != NULL);

	return read_database(path);
#endif
}

/* see comment in smaunch-smack.h */
int smaunch_smack_has_database()
{
	return ISVALID(last_item);
}

/* see comment in smaunch-smack.h */
void smaunch_smack_set_subject(const char *subject)
{
	assert(subject != NULL);
	assert(smack_subject_is_valid(subject));

	current_subject = subject;
}

/* see comment in smaunch-smack.h */
int smaunch_smack_has_key(const char *key)
{
	int keyi;

	assert(key);
	assert(smaunch_smack_has_database());

	keyi = item_get(key, 0);
	if (!ISVALID(keyi))
		return 0;

	if (!ISVALID($(keyi)[item_rules]))
		return 0;

	return 1;
}

/* see comment in smaunch-smack.h */
void smaunch_smack_context_start()
{
	assert(smaunch_smack_has_database());
	memset(coda_context, 0, object_count * sizeof * coda_context);
}

/* see comment in smaunch-smack.h */
int smaunch_smack_context_add(const char *key)
{
	int keyi;

	assert(key);
	assert(smaunch_smack_has_database());

	keyi = item_get(key, 0);
	if (!ISVALID(keyi))
		return keyi;

	if (!ISVALID($(keyi)[item_rules]))
		return -EINVAL;

	add_context_key(coda_context, keyi);
	return 0;
}

/* see comment in smaunch-smack.h */
int smaunch_smack_context_apply()
{
#if SIMULATION
	/* for simulation, print on stdout(1) multiline(1) differences(1) */
	assert(smaunch_smack_has_database());
	return context_apply(coda_context, 1, 1, 1);
#else
	int file, result;
	size_t len;
	char path[PATH_MAX];
	const char *smount;
	static const char itf[] = "/load-self2";

	assert(smaunch_smack_has_database());

	/* get the mount point */
	smount = smack_fs_mount_point();
	if (!smount)
		return -EACCES;

	/* compute the load-self2 interface path */
	len = strlen(smount);
	if (len + sizeof itf > sizeof path)
		return -ENAMETOOLONG;
	memcpy(path, smount, len);
	memcpy(path + len, itf, sizeof itf);

	/* open the load-self2 interface */
	file = open(path, O_WRONLY);
	if (file < 0)
		return -errno;

	/* write the rules */
	result = context_apply(coda_context, file, 1, 1);
	close(file);
	return result;
#endif
}

/* see comment in smaunch-smack.h */
int smaunch_smack_dump_all(int file)
{
	assert(smaunch_smack_has_database());
	return context_apply(coda_reference, file, 1, 0);
}


#ifdef TEST
void cdump(const char *name, smack_coda *context)
{
	int i;

	if(context) {
		printf("%s",name);
		for(i=0;i<object_count;i++)
			printf(" [%s %d]",(char*)($(object_strings[i])),context[i]);
		printf("\n");
	}
}

void dump()
{
	int i, j;

	i = last_item;
	while(ISVALID(i)) {
		printf("[%d]", i);
		printf(" %s", (char*)($($(i)[item_string])));
		printf("#%d:", $(i)[item_hash]);
		if(ISVALID($(i)[item_object]))
			printf(" obj=%d", $(i)[item_object]);
		else
			printf(" <no-obj>");
		j = $(i)[item_rules];
		while(object_strings && ISVALID(j)) {
			printf(" [%s %d]",(char*)($(object_strings[$(j)[rule_object]])),$(j)[rule_access]);
			j = $(j)[rule_previous];
		}
		printf("\n");
		i = $(i)[item_previous];
	}
	cdump("REF",coda_reference);
	cdump("CTX",coda_context);
	printf("\n");
}
#endif

