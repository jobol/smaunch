/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#include <stdlib.h>
#include <assert.h>

#include "smack-utils-label.h"


int smack_label_is_valid(const char *text)
{
	char c;
	int len;

	assert(text != NULL);

	len=0;
	c = text[len];
	if (c=='-')
		return 0;

	for(;;) {
		if (c <= ' ' || c > '~')
			return 0;

		switch(c) {
		case '/':
		case '\\':
		case '\'':
		case '"':
			return 0;
		}
		c = text[len++];

		if (!c)
			return 1;

		if (len > 255)
			return 0;
	}
}

int smack_object_is_valid(const char *text)
{
	return smack_label_is_valid(text);
}


int smack_subject_is_valid(const char *text)
{
	return smack_label_is_valid(text);
}


