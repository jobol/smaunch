#include <stdlib.h>
#include <assert.h>

#include "smack-utils-coda.h"
#include "smack-utils-access.h"




int smack_access_is_valid(smack_access access)
{
	if (access & ~smack_access_full_mask)
		return 0;
	if ((access >> smack_access_shift_count) & access)
		return 0;
	return 1;
}


smack_coda smack_access_allow_coda(smack_access access)
{
	int r;

	assert(smack_access_is_valid(access));

	r = access & smack_access_mask;

	assert(smack_coda_is_valid(r));

	return r;
}

smack_coda smack_access_deny_coda(smack_access access)
{
	int r;

	assert(smack_access_is_valid(access));

	r = (access >> smack_access_shift_count) & smack_access_mask;

	assert(smack_coda_is_valid(r));

	return r;
}

/*
Create an access from the two coda 'allow' and 'deny'.

'allow' and 'deny' MUST be valid codas has cheched by
function "smack_coda_is_valid".

Returns: the built access.
*/
int smack_access_make(smack_coda allow, smack_coda deny)
{
	int r;

	assert(smack_coda_is_valid(allow));
	assert(smack_coda_is_valid(deny));
	assert((allow & deny) == 0);

	r = (deny << smack_access_shift_count) | allow;

	assert(smack_access_is_valid(r));
	assert(smack_access_allow_coda(r) == allow);
	assert(smack_access_deny_coda(r) == deny);

	return r;
}








int smack_access_compose(smack_access first, smack_access second)
{
	int r, m;

	assert(smack_access_is_valid(first));
	assert(smack_access_is_valid(second));

	/* don't apply masks because the result is the same... */
	m = (second << smack_access_shift_count) | (second >> smack_access_shift_count);
	r = (first & ~m) | second;

	assert(smack_access_is_valid(r));
	assert(smack_access_allow_coda(r) == ((smack_access_allow_coda(first) & ~smack_access_deny_coda(second)) | smack_access_allow_coda(second)));
	assert(smack_access_deny_coda(r) == ((smack_access_deny_coda(first) & ~smack_access_allow_coda(second)) | smack_access_deny_coda(second)));

	return r;
}

int smack_access_strings_are_valid(const char *allow, const char *deny)
{
	assert(allow != NULL);
	if (!smack_coda_string_is_valid(allow))
		return 0;
	if (deny != NULL && !smack_coda_string_is_valid(deny))
		return 0;
	return 1;
}

smack_access smack_access_from_strings(const char *allow, const char *deny)
{
	int r;

	assert(allow != NULL);
	assert(smack_access_strings_are_valid(allow,deny));

	if (deny == NULL) {
		r = smack_coda_from_string(allow);
		r = r | ((~r & smack_access_mask) << smack_access_shift_count);
	} else {
		r = smack_coda_from_string(deny);
		r = (smack_coda_from_string(allow) & ~r) | (r << smack_access_shift_count);
	}

	assert(smack_access_is_valid(r));
	assert(smack_access_allow_coda(r)==smack_coda_from_string(allow));
	assert(deny==NULL || smack_access_deny_coda(r)==smack_coda_from_string(deny));
	assert(deny!=NULL || !smack_access_is_partial(r));

	return r;
}

int smack_access_is_partial(smack_access access)
{
	int r;

	assert(smack_access_is_valid(access));

	r = (((access >> smack_access_shift_count) | access) & smack_access_mask) != smack_access_mask;

	assert(r == ((smack_access_allow_coda(access)|smack_access_deny_coda(access))!=smack_access_mask));

	return r;
}

int smack_access_string_length(smack_access access)
{
	int r;

	assert(smack_access_is_valid(access));

	r = smack_coda_string_length(smack_access_allow_coda(access));
	if (smack_access_is_partial(access))
		r += 1 + smack_coda_string_length(smack_access_deny_coda(access));

	assert(1 <= r && r <= 1+2*smack_coda_bits_count);
	return r;
}

int smack_access_to_string(smack_access access, char *text, int length)
{
	int r;

	assert(smack_access_is_valid(access));
	assert(text != NULL);
	assert(smack_access_string_length(access) <= length);

	r = smack_coda_to_string(smack_access_allow_coda(access), text, length);
	if (smack_access_is_partial(access)) {
		text[r++] = ' ';
		r += smack_coda_to_string(smack_access_deny_coda(access), text+r, length-r);
	}

	assert(r == smack_access_string_length(access));
	assert(r <= length);
	return r;
}

smack_access smack_access_fullfill(smack_access access)
{
	int r;

	assert(smack_access_is_valid(access));

	r = access & smack_access_mask;
	r = r | ((~r & smack_access_mask) << smack_access_shift_count);

	assert(!smack_access_is_partial(r));
	assert(smack_access_allow_coda(access)==smack_access_allow_coda(r));
	return r;
}








