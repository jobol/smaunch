/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include "parse.h"

/* 
 * Add the character 'c' to the line data of 'parse'.
 * Return 0 in case of success or a composed error indicator.
 */
static int add_char(struct parse *parse, char c)
{
	if (parse->linepos >= sizeof parse->line) {
		parse->finished = parse_make_syntax_error(parse_line_too_long,parse->lino);
		return parse->finished;
	}

	parse->line[parse->linepos++] = c;
	return 0;
}

/*
 * Add the terminating zero to the current field and increment
 * the count of read fields.
 * Return 0 in case of success or a composed error indicator.
 */
static int end_field(struct parse *parse)
{
	int result;

	result = add_char(parse, 0);
	if (result == 0)
		parse->fieldcount++;
	return result;
}

/*
 * Begin a field whose first character is 'c'.
 */
static int begin_field(struct parse *parse, char c)
{
	if (parse->fieldcount >= parse_field_count) {
		parse->finished = parse_make_syntax_error(parse_too_much_fields,parse->lino);
		return parse->finished;
	}

	parse->fields[parse->fieldcount] = parse->line + parse->linepos;
	return add_char(parse, c);
}

int parse_line(struct parse *parse)
{
	int state, sts;
	ssize_t readen;
	char c;

	assert(parse);
	assert(!parse_has_error(parse));
	assert(!parse->finished);

	state = 0;
	parse->linepos = 0;
	parse->fieldcount = 0;
	parse->begsp = 0;
	parse->lino++;
	for(;;) {
		while (parse->bufpos < parse->bufcount) {
			c = parse->buffer[parse->bufpos++];
			switch (state) {
			case 0: /* begin of line */
				switch(c) {
				case ' ':
				case '\t':
					parse->begsp = 1;
					break;
				case '\n':
					parse->begsp = 0;
					parse->lino++;
					break;
				case '-':
					state = 1;
					break;
				default:
					sts = begin_field(parse, c);
					if (sts)
						return sts;
					state = 3;
					break;
				}
				break;

			case 1: /* begin of comment */
				if (c == '-')
					state = 2; /* yes, its a comment */
				else {
					/* the field was starting with a dash */
					sts = begin_field(parse, '-');
					if (sts)
						return sts;
					switch(c) {
					case ' ':
					case '\t':
					case '\n':
						sts = end_field(parse);
						if (sts)
							return sts;
						if (c == '\n')
							return 0;
						state = 4;
						break;
					default:
						sts = add_char(parse, c);
						if (sts)
							return sts;
						state = 3;
						break;
					}
				}
				break;

			case 2: /* comment */
				if (c == '\n') {
					if (parse->fieldcount)
						return 0;
					parse->begsp = 0;
					parse->lino++;
					state = 0;
				}
				break;

			case 3: /* field */
				switch(c) {
				case ' ':
				case '\t':
					sts = end_field(parse);
					if (sts)
						return sts;
					state = 4;
					break;
				case '\n':
					sts = end_field(parse);
					if (sts)
						return sts;
					return 0;
				default:
					sts = add_char(parse, c);
					if (sts)
						return sts;
					break;
				}
				break;

			case 4: /* after field */
				switch(c) {
				case ' ':
				case '\t':
					break;
				case '\n':
					return 0;
				case '-':
					state = 1;
					break;
				default:
					sts = begin_field(parse, c);
					if (sts)
						return sts;
					state = 3;
					break;
				}
				break;

			}
		}

		/* read the buffer */
		assert(parse->bufpos == parse->bufcount);
		readen = read(parse->file, parse->buffer, sizeof parse->buffer);
		if (readen < 0) {
			parse->finished = -errno;
			return parse->finished;
		}

		/* end of file */
		if (!readen) {
			switch(state) {
			case 1: /* begin of comment */
				sts = begin_field(parse, '-');
				if (sts)
					return sts;
			case 3: /* field */
				sts = end_field(parse);
				if (sts)
					return sts;
			}
			parse->finished = 1;
			return 0;
		}

		/* reset reads */
		assert(readen <= INT_MAX);
		parse->bufpos = 0;
		parse->bufcount = (int)readen;
	}
}

int parse_has_error(struct parse *parse)
{
	assert(parse);

	return parse->finished < 0;
}

void parse_init(struct parse *parse, int file)
{
	assert(parse);

	parse->file = file;
	parse->finished = 0;
	parse->lino = 0;
	parse->bufcount = 0;
	parse->bufpos = 0;
	parse->linepos = 0;
	parse->fieldcount = 0;
	parse->begsp = 0;
}

int parse_init_open(struct parse *parse, const char *path)
{
	int file;

	assert(parse);

	file = open(path, O_RDONLY);
	if (file < 0)
		return -errno;

	parse_init(parse, file);
	return 0;
}


#ifdef TEST_PARSE
#include <stdio.h>
int main(int argc, char **argv)
{
	struct parse parse;
	parse_init(&parse, 0);
	while(!parse.finished) {
		int sts = parse_line(&parse);
		int i;
		if (sts)
			printf("error %d\n",sts);
		else {
			printf("ok l=%d k=%d b=%d f=%d:",parse.lino,parse.fieldcount, parse.begsp, parse.finished);
			for (i = 0 ; i < parse.fieldcount ; i++)
				printf(" %d='%s'",i+1,parse.fields[i]);
			printf("\n");
		}
	}
	return 0;
}
#endif
