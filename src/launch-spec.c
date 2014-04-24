/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

enum {
	launch_spec_max_key_count = 128,
	launch_spec_max_subst_count = 128,
};

struct launch_spec {
	int nkeys;
	int nsubsts;
	const char *exec_target;
	const char *keys[launch_spec_max_subst_count];
	const char *substs[launch_spec_max_subst_count][2];
};

void launch_spec_init(struct launch_spec *spec)
{
	spec->nkeys = 0;
	spec->nsubsts = 0;
	spec->exec_target = 0;
}

int launch_spec_parse(struct launch_spec *spec, char *buffer)
{
	int pos;

	assert(spec);
	assert(buffer);

	/* parse */
	pos = 0;
	while (buffer[pos]) {
		switch (buffer[pos]) {
		case '!':
		case '+':
		case '*':
		case '=':
		case '-':
			/* a key */
			if (spec->nkeys >= sizeof spec->keys / sizeof spec->keys[0])
				return -E2BIG;

			spec->keys[spec->nkeys++] = buffer + pos++;
			if (!buffer[pos] || buffer[pos] == '\n')
				return -EINVAL;
			while (buffer[pos]) {
				if (buffer[pos++] == '\n') {
					buffer[pos - 1] = 0;
					break;
				}
			}
			break;

		case '%':
			/* a substitution */
			if (spec->nsubsts >= sizeof spec->substs / sizeof spec->substs[0])
				return -E2BIG;

			spec->substs[spec->nsubsts][0] = buffer + pos++;
			if (!buffer[pos] || buffer[pos] == '\n')
				return -EINVAL;
			while (buffer[pos]) {
				if (buffer[pos] == '\n')
					return -EINVAL;
				if (buffer[pos++] == '=') {
					buffer[pos - 1] = 0;
					break;
				}
			}

			spec->substs[spec->nsubsts][1] = buffer + pos;
			while (buffer[pos]) {
				if (buffer[pos++] == '\n') {
					buffer[pos - 1] = 0;
					break;
				}
			}
			break;

		case '@':
			/* the target */
			if (spec->exec_target)
				return -EINVAL;
			spec->exec_target = buffer + ++pos;
			if (!buffer[pos] || buffer[pos] == '\n')
				return -EINVAL;
			while (buffer[pos]) {
				if (buffer[pos++] == '\n') {
					buffer[pos - 1] = 0;
					break;
				}
			}
			break;

		default:
			return -EINVAL;
		}
	}
	
	return 0;
}

int launch_spec_generate(struct launch_spec *spec, char **buffer, int *length)
{
	int i;
	int size;
	char *w;
	const char *r;

	assert(spec);
	assert(buffer);
	assert(length);

	/* compute the needed size */
	if (!spec->exec_target)
		return -EINVAL;

	size = 2 + (int)strlen(spec->exec_target);

	i = spec->nkeys;
	while (i) {
		if (!spec->keys[--i])
			return -EINVAL;

		switch(*spec->keys[i]) {
		case '!':
		case '+':
		case '*':
		case '=':
		case '-':
			break;
		default:
			return -EINVAL;
		}

		size += 1 + (int)strlen(spec->keys[i]);
	}

	i = spec->nsubsts;
	while (i) {
		i--;
		if (!spec->substs[i][0] || spec->substs[i][0][0] != '%' || !spec->substs[i][1])
			return -EINVAL;

		size += 2 + (int)strlen(spec->substs[i][0]) + (int)strlen(spec->substs[i][1]);
	}

	/* allocate */
	w = malloc(size+1);
	if (!w)
		return -ENOMEM;

	/* fill */
	*buffer = w;
	*length = size;

	*w++ = '@';
	r = spec->exec_target;
	while(*r) *w++ = *r++;
	*w++ = '\n';

	i = spec->nkeys;
	while (i) {
		r = spec->keys[--i];
		while(*r) *w++ = *r++;
		*w++ = '\n';
	}

	i = spec->nsubsts;
	while (i) {
		r = spec->substs[i][0];
		while(*r) *w++ = *r++;
		*w++ = '=';
		r = spec->substs[i][1];
		while(*r) *w++ = *r++;
		*w++ = '\n';
	}
	assert(size == (int)(w - *buffer));
	*w = 0;

	return 0;
}

int launch_spec_get_keys(struct launch_spec *spec, const char **keys, int count)
{
	int	r, w, n;

	n = spec->nkeys;
	if (n >= count)
		return -E2BIG;

	r = 0;
	w = 0;
	while (r < n) {
		switch (spec->keys[r][0]) {
		case '!':
		case '+':
		case '*':
		case '=':
			if (w >= count)
				return -E2BIG;
			keys[w++] = spec->keys[r];
			break;
		case '-':
			break;
		default:
			/* not needed, should be an error, but ... */
			if (w >= count)
				return -E2BIG;
			keys[w++] = spec->keys[r];
			break;
		}
		r++;
	}
	if (w >= count)
		return -E2BIG;
	keys[w] = 0;

	return w;
}



