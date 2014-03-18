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

#if !defined(SIMULATION)
#define SIMULATION 0
#endif

#define INVALID    (-1)
#define ISVALID(x) ((x)>=0)
#define $(offset)  (data_buffer.data + (offset))
#define H          hash_buffer.data

#define ALLOWCOMPILE 1

enum item_offsets {
	item_hash = 0,
	item_string = 1,
	item_rules = 2,
	item_object = 3,
	item_previous = 4,
	item_size = 5
};

enum rule_offsets {
	rule_object = 0,
	rule_access = 1,
	rule_previous = 2,
	rule_size = 3
};


static const char *current_loaded_database = 0;
static smack_coda *coda_reference = 0;
static smack_coda *coda_context = 0;
static int *object_strings = 0;

static int object_count = 0;
static int item_count = 0;
static int last_item = INVALID;
static struct buffer hash_buffer = { 0, 0, 0 };
static struct buffer data_buffer = { 0, 0, 0 };


#ifdef TEST
void cdump(const char *name, smack_coda *context);
void dump();
#endif

/* clear the database */
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
	current_loaded_database = 0;
}














static int item_set_get(const char *text, int create)
{
	int hash, len, iter, indic, istr, i, result;

	/* make the hash */
	for (len = 0, hash = 5381 ; text[len] ; len++)
		hash = hash * 33 + (unsigned)text[len];
	hash &= INT_MAX;

	/* search */
	if (hash_buffer.count) {
		iter = hash % hash_buffer.count;
		result = H[iter];
		assert(!ISVALID(result) || (0 <= result && result < data_buffer.count));
		while (ISVALID(result)) {
			if ($(result)[item_hash] == hash) {
				istr = $(result)[item_string];
				if (0 == strcmp(text, (char*)($(istr))))
					return result;
			}
			iter = (iter ? iter : hash_buffer.count) - 1;
			result = H[iter];
			assert(!ISVALID(result) || (0 <= result && result < data_buffer.count));
		}
	}

	/* fail if create not set */
	if (!create)
		return -ENOENT;

	/* grow if needed */
	if ((item_count + 1) > (hash_buffer.count >> 2)) {
		/* extend the buffer */
		i = buffer_set_count(&hash_buffer, hash_buffer.count + 1024);
		if (i < 0)
			return i;

		/* reinit the buffer */
		i = hash_buffer.count;
		while (i)
			H[--i] = INVALID;

		/* records the keys */
		i = last_item;
		while (ISVALID(i)) {
			iter = $(i)[item_hash] % hash_buffer.count;
			indic = H[iter];
			assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
			while (ISVALID(indic)) {
				iter = (iter ? iter : hash_buffer.count) - 1;
				indic = H[iter];
				assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
			}
			H[iter] = i;
			i = $(i)[item_previous];
		}
	}

	/* create the new key */
	result = buffer_alloc(&data_buffer, item_size);
	if (result < 0)
		return result;
	istr = buffer_strndup(&data_buffer, text, len);
	if (istr < 0)
		return istr;

	$(result)[item_hash] = hash;
	$(result)[item_string] = istr;
	$(result)[item_rules] = INVALID;
	$(result)[item_object] = INVALID;
	$(result)[item_previous] = last_item;
	last_item = result;
	item_count++;

	/* insert the result */
	iter = hash % hash_buffer.count;
	indic = H[iter];
	assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
	while (ISVALID(indic)) {
		iter = (iter ? iter : hash_buffer.count) - 1;
		indic = H[iter];
		assert(!ISVALID(indic) || (0 <= indic && indic < data_buffer.count));
	}
	H[iter] = result;

	return result;
}










static int add_rule_key(int keyi, const char *object, const char *access)
{
	int obji, rulei;

	assert(keyi >= 0);
	assert(object);
	assert(access);
	assert(smack_object_is_valid(object));
	assert(smack_coda_string_is_valid(access));
	assert(!coda_reference);

	/* get the object */
	obji = item_set_get(object, 1);
	if (obji < 0)
		return obji;

	if (!ISVALID($(obji)[item_object])) {
		$(obji)[item_object] = object_count++;
	}

	/* grow data of the key */
	rulei = buffer_alloc(&data_buffer, rule_size);
	if (rulei < 0)
		return rulei;

	$(rulei)[rule_object] = $(obji)[item_object];
	$(rulei)[rule_access] = (int)smack_coda_from_string(access);
	$(rulei)[rule_previous] = $(keyi)[item_rules];
	$(keyi)[item_rules] = rulei;
	
	return 0;
}

static void add_context_key(smack_coda *set, int keyi)
{
	int rulei, obji;
	smack_coda coda;

	assert(set);
	assert(0 <= keyi);
	assert(keyi < data_buffer.count);
	assert(coda_reference);

	/* apply the key data to the set */
	rulei = $(keyi)[item_rules];
	while (ISVALID(rulei)) {
		obji = $(rulei)[rule_object];
		assert(ISVALID(obji));
		assert(0 <= obji);
		assert(obji < object_count);
		coda = (smack_coda)$(rulei)[rule_access];
		assert(smack_coda_is_valid(coda));
		set[obji] |= coda;
		rulei = $(rulei)[rule_previous];
	}
}

static int context_apply(const smack_coda *codas, int file, int multi, int diff)
{
	char buffer[8192], *rule;
	unsigned index, pos, lr, wr;
	int state;
	smack_coda coda;
	ssize_t sts;

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
			assert(lr + 8 <= sizeof buffer);

			/* flush if needed */
			if (pos + lr + 8 > sizeof buffer) {
				state = 12;
				break;
			}

		case 2:
			/* fill the buffer */
			assert(pos + lr + 8 <= sizeof buffer);
			memcpy(buffer + pos, rule, lr);
			pos += lr;
			buffer[pos++] = ' ';
			pos += smack_coda_to_string(coda, buffer + pos, 6);
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
						return -1;
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


















static int compile_database(const char *path)
{
	struct parse parse;
	int sts, keyi;

	sts = parse_init_open(&parse, path);
	if (sts < 0)
		return sts;

	keyi = -1;
	while(!parse.finished) {

		/* read one line */
		sts = parse_line(&parse);
		if (sts < 0) {
			close(parse.file);
			return sts;
		}

		switch (parse.fieldcount) {
		case 0: /* nothing (may append at end) */
			break;

		case 1: /* key */
			if (parse.begsp) {
				close(parse.file);
				return parse_make_syntax_error(smack_object_without_access,parse.lino);
			}
			sts = item_set_get(parse.fields[0], 1);
			if (sts < 0) {
				close(parse.file);
				return sts;
			}
			keyi = sts;
			break;

		case 2: /* object / access */
			if (!parse.begsp) {
				close(parse.file);
				return parse_make_syntax_error(smack_extra_after_key,parse.lino);
			}
			/* check context */
			if (keyi < 0) {
				close(parse.file);
				return parse_make_syntax_error(smack_no_key_set, parse.lino);
			}
			if (!smack_object_is_valid(parse.fields[0])) {
				close(parse.file);
				return parse_make_syntax_error(smack_invalid_object, parse.lino);
			}
			if (!smack_coda_string_is_valid(parse.fields[1])) {
				close(parse.file);
				return parse_make_syntax_error(smack_invalid_access, parse.lino);
			}
			/* add the rule */
			sts = add_rule_key(keyi, parse.fields[0], parse.fields[1]);
			if (sts < 0) {
				close(parse.file);
				return sts;
			}
			break;

		default:
			close(parse.file);
			return parse_make_syntax_error(smack_extra_after_access,parse.lino);
		}
	}

	/* allocate the codas */
	coda_reference = calloc(object_count, sizeof * coda_reference);
	if (!coda_reference)
		return -ENOMEM;
	coda_context = calloc(object_count, sizeof * coda_context);
	if (!coda_context)
		return -ENOMEM;
	object_strings = calloc(object_count, sizeof * object_strings);
	if (!object_strings)
		return -ENOMEM;

	/* init the coda of reference */
	keyi = last_item;
	while (ISVALID(keyi)) {
		if (ISVALID($(keyi)[item_rules])) 
			add_context_key(coda_reference, keyi);
		if (ISVALID($(keyi)[item_object])) 
			object_strings[$(keyi)[item_object]] = $(keyi)[item_string];
		keyi = $(keyi)[item_previous];
	}

	return 0;
}

#if ALLOWCOMPILE
static int save_compiled_database(const char *path)
{
	int fd, head[5], err;
	struct iovec iovect[5];
	ssize_t ret;

	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
	if (fd < 0)
		return fd;

	head[0] = data_buffer.count;
	head[1] = hash_buffer.count;
	head[2] = item_count;
	head[3] = object_count;
	head[4] = last_item;

	iovect[0].iov_base = head;
	iovect[0].iov_len = sizeof head;

	iovect[1].iov_base = data_buffer.data;
	iovect[1].iov_len = data_buffer.count * sizeof(int);

	iovect[2].iov_base = hash_buffer.data;
	iovect[2].iov_len = hash_buffer.count * sizeof(int);
	
	iovect[3].iov_base = coda_reference;
	iovect[3].iov_len = object_count * sizeof * coda_reference;

	iovect[4].iov_base = object_strings;
	iovect[4].iov_len = object_count * sizeof * object_strings;

	ret = writev(fd, iovect, 5);
	err = ret < 0 ? -errno : 0;
	close(fd);
	return err;
}

static int read_compiled_database(const char *path)
{
	int fd, head[5], err;
	struct iovec iovect[4];
	ssize_t ret;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return fd;

	ret = read(fd, head, sizeof head);
	if (ret < 0) {
		err = -errno;
		close(fd);
		return err;
	}
	if (ret < sizeof head) {
		close(fd);
		return -EINTR;
	}

	data_buffer.data = malloc(head[0] * sizeof(int));
	hash_buffer.data = malloc(head[1] * sizeof(int));
	coda_reference = malloc(head[3] * sizeof * coda_reference);
	coda_context = calloc(head[3], sizeof * coda_reference);
	object_strings = malloc(head[3] * sizeof * object_strings);

	if (!data_buffer.data || !hash_buffer.data || !coda_reference || !coda_context) {
		close(fd);
		return -ENOMEM;
	}

	data_buffer.count = head[0];
	data_buffer.capacity = head[0];
	hash_buffer.count = head[1];
	hash_buffer.capacity = head[1];
	item_count = head[2];
	object_count = head[3];
	last_item = head[4];

	iovect[0].iov_base = data_buffer.data;
	iovect[0].iov_len = data_buffer.count * sizeof(int);

	iovect[1].iov_base = hash_buffer.data;
	iovect[1].iov_len = hash_buffer.count * sizeof(int);
	
	iovect[2].iov_base = coda_reference;
	iovect[2].iov_len = object_count * sizeof * coda_reference;

	iovect[3].iov_base = object_strings;
	iovect[3].iov_len = object_count * sizeof * object_strings;

	ret = readv(fd, iovect, 4);
	err = ret < 0 ? -errno : 0;
	close(fd);
	return err;
}



static int compute_compiled_database_name(char *result, size_t length, const char *path, const struct stat *stat)
{
	int ret;

	/* get name of the compiled database */
/*
	ret = snprintf(result, length, "/tmp/smaunch:%d:%d", (int)stat.st_dev, (int)stat.st_ino);
*/
	ret = snprintf(result, length, "%s.compiled", path);
	if (ret >= (int)length)
		return -ENAMETOOLONG;

	return 0;
}


int smaunch_smack_load_database(const char *path)
{
	char cpldb[PATH_MAX];
	struct stat sdb, scpl;
	int ret;

	assert(path != NULL);

	/* checking if already loaded */
	if (current_loaded_database) {
		if (0 == strcmp(current_loaded_database, path))
			return 0;
		clear_database();
	}

	/* get info about database */
	ret = stat(path, &sdb);
	if (ret < 0)
		return -errno;

	/* get name of the compiled database */
	ret = compute_compiled_database_name(cpldb, sizeof cpldb, path, &sdb);
	if (ret < 0)
		return ret;

	/* test if available */
	ret = stat(cpldb, &scpl);
	if (ret < 0 || sdb.st_mtime >= scpl.st_mtime) {
		ret = compile_database(path);
		if (ret < 0)
			clear_database();
		else {
			save_compiled_database(cpldb);
			current_loaded_database = path;
		}
	} else {
		ret = read_compiled_database(cpldb);
		if (ret < 0)
			clear_database();
		else
			current_loaded_database = path;
	}

	return ret;
}
#else
int smaunch_smack_load_database(const char *path)
{
	int ret;

	assert(path != NULL);

	/* checking if already loaded */
	if (current_loaded_database) {
		if (0 == strcmp(current_loaded_database, path))
			return 0;
		clear_database();
	}

	ret = compile_database(path);
	if (ret < 0)
		clear_database();
	else
		current_loaded_database = path;

	return ret;
}
#endif

int smaunch_smack_has_database()
{
	return !!current_loaded_database;
}

int smaunch_smack_has_key(const char *key)
{
	int keyi;

	assert(key);
	assert(smaunch_smack_has_database());

	keyi = item_set_get(key, 0);
	if (!ISVALID(keyi))
		return 0;

	if (!ISVALID($(keyi)[item_rules]))
		return 0;

	return 1;
}

void smaunch_smack_context_start()
{
	assert(smaunch_smack_has_database());
	memset(coda_context, 0, object_count * sizeof * coda_context);
}

int smaunch_smack_context_add(const char *key)
{
	int keyi;

	assert(key);
	assert(smaunch_smack_has_database());

	keyi = item_set_get(key, 0);
	if (!ISVALID(keyi))
		return keyi;

	if (!ISVALID($(keyi)[item_rules]))
		return -EINVAL;

	add_context_key(coda_context, keyi);
	return 0;
}


int smaunch_smack_context_apply()
{
#if SIMULATION
	assert(smaunch_smack_has_database());
	return context_apply(coda_context, 1, 1, 1);
#else
	int file, multiline, all, result;
	size_t len;
	char path[PATH_MAX];
	const char *smount;
	static const char itf[] = "/load-self2";

	assert(smaunch_smack_has_database());

	smount = smack_fs_mount_point();
	if (!smount)
		return -EACCES;

	len = strlen(smount);
	if (len + sizeof itf > sizeof path)
		return -ENAMETOOLONG;
	memcpy(path, smount, len);
	memcpy(path + len, itf, sizeof itf);

	file = open(path, O_WRONLY);
	if (file < 0)
		return -errno;

	multiline = 0;
	all = 1;
	result = context_apply(coda_context, file, multiline, !all);
	close(file);
	return result;
#endif
}


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

