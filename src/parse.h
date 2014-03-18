/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * This parser read files made a space (or tab) separated fields.
 * The comments are started with two successives dash -- as this.
 * Lines without a not empty field after comment removal are ignored.
 */

/*
 * Definition of parser constants.
 */
enum parse_constants {
	parse_field_count = 32,			/* the maximum count of fields */
	parse_buffer_length = 16384,		/* the read buffer size */
	parse_line_length = 8192		/* the line buffer size */
};

/*
 * Definition of the parse structure.
 */
struct parse {
	int file;							/* the file read */
	int finished;						/* set to non-zero at end (1 for normal end or the negative error code) */
	int lino;							/* the line number being read */
	int bufcount;						/* the count of data in read buffer */
	int bufpos;							/* position of read in read buffer */
	int linepos;						/* position of write in line buffer */
	int fieldcount;						/* count of fields for the line read */
	int begsp;							/* are there some space(s) (or tab) at start of the line read */
	char *fields[parse_field_count];	/* the fields of the current line read as string pointers */
	char line[parse_line_length];		/* the data of the fields */
	char buffer[parse_buffer_length];	/* the read buffer */
};

/*
 * Definition of the raised errors
 */
enum parse_syntax_errors {
	parse_line_too_long = 1,		/* line too long, see {parse_constants}.parse_line_length */
	parse_too_much_fields = 2		/* too much fields, see {parse_constants}.parse_field_count */
};

/*
 * Macros for handling syntax errors:
 *  - parse_make_syntax_error    create a syntax error code of number 'err' for line 'li'
 *  - parse_is_syntax_error      test if 'err' is a syntax error code
 *  - parse_syntax_error_number  get the error number of the syntax error code
 *  - parse_syntax_error_line    get the line number of the syntax error code
 */
#define parse_syntax_error_number(err)  ((-(err)) >> 20)
#define parse_syntax_error_line(err)    ((-(err)) & 0xfffff)
#define parse_is_syntax_error(err)      ((err)<0 && parse_syntax_error_number(err) > 0)
#define parse_make_syntax_error(err,li) (-(((err) << 20) | ((li) & 0xfffff)))

/*
 * Read/parse a new line line of fields.
 *
 * Requires that 'parse->finished' is null (zero) meaning
 * not finished and no error.
 *
 * Returns 0 if succeful or an error code otherwise.
 *
 * After returning 0, the caller will check
 * 'parse->fieldcount', 'parse->begsp' and 'parse->fields'
 * to get the data. 
 *
 * 'parse->fieldcount' can be zero at end of file but it is greater
 * than zero otherwise.
 *
 * At end of file the field 'parse->finished' will be set to 1.
 * On error the field 'parse->finished' will be set to error number (a negative value).
 */
int parse_line(struct parse *parse);

/*
 * Is the 'parse' structure recording an error?
 * Returns 0 if not error is recorded or 1 otherwise.
 */
int parse_has_error(struct parse *parse);

/*
 * Init the 'parse' structure for reading the opened 'file'
 */
void parse_init(struct parse *parse, int file);

/*
 * Open the file of 'path' and init the 'parse' structure to read
 * that opened file.
 *
 * Return 0 on success or a negative error code if opening file failed.
 */
int parse_init_open(struct parse *parse, const char *path);


