/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#include <stdlib.h>
#include <assert.h>

#include "smack-utils-coda.h"

static char codachars[smack_coda_bits_count] = { 'l', 't', 'a', 'x', 'w', 'r' };

static int char2coda(char c)
{
#define lower_case_of(c)   ((c)|' ')

	int r;

	switch(lower_case_of(c)) {
	case 'a': r = smack_coda_a; break;
	case 'l': r = smack_coda_l; break;
	case 'r': r = smack_coda_r; break;
	case 't': r = smack_coda_t; break;
	case 'w': r = smack_coda_w; break;
	case 'x': r = smack_coda_x; break;
	default: r = 0; break;
	}

	return r;

#undef lower_case_of
}


int smack_coda_string_is_valid(const char *text)
{
	assert(text != NULL);

	for(;*text;text++)
		if(*text!='-' && !char2coda(*text))
			return 0;

	return 1;
}



int smack_coda_is_valid(smack_coda coda)
{
	return coda == (coda & smack_coda_bits_mask);
}

smack_coda smack_coda_complement(smack_coda coda)
{
	assert(smack_coda_is_valid(coda));

	return (~coda) & smack_coda_bits_mask;
}

smack_coda smack_coda_from_string(const char *text)
{
	int r;

	assert(text != NULL);
	assert(smack_coda_string_is_valid(text));

	for(r=0 ; *text ; text++)
		if (*text != '-')
			r |= char2coda(*text);

	assert(smack_coda_is_valid(r));

	return r;
}

int smack_coda_string_length(smack_coda coda)
{
	int r, c;

	assert(smack_coda_is_valid(coda));

	if (!coda) {
		r = 1;
	} else {
		r = 0;
		c = smack_coda_bits_count;
		while (c) {
			c--;
			r += coda & 1;
			coda >>= 1;
		}
	}

	assert(1 <= r && r <= smack_coda_bits_count);

	return r;
}

int smack_coda_to_string(smack_coda coda, char *text, int length)
{
	int r, c;

	assert(text != NULL && length >= smack_coda_string_length(coda));

	if (!coda) {
		text[0] = '-';
		r = 1;
	} else {
		r = 0;
		c = smack_coda_bits_count;
		while (c)
			if (1 & (coda >> --c))
				text[r++] = codachars[c];
	}

	assert(r == smack_coda_string_length(coda));
	assert(r <= length);
	return r;
}



