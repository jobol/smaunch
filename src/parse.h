/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
enum parse_constants {
	parse_key_count = 32,
	parse_buffer_length = 8192,
	parse_line_length = 8192
};

struct parse {
	int file;
	int finished;
	int lino;
	int bufcount;
	int bufpos;
	int linepos;
	int keycount;
	int begsp;
	char *keys[parse_key_count];
	char line[parse_line_length];
	char buffer[parse_buffer_length];
};


enum parse_syntax_errors {
	parse_line_too_long = 1,
	parse_too_much_keys = 2
};

#define parse_syntax_error_number(err)  ((-(err)) >> 20)
#define parse_syntax_error_line(err)    ((-(err)) & 0xfffff)
#define parse_is_syntax_error(err)      ((err)<0 && parse_syntax_error_number(err) > 0)

#define parse_make_syntax_error(err,li) (-((err << 20) | li))



int parse_line(struct parse *parse);

int parse_has_error(struct parse *parse);

void parse_init(struct parse *parse, int file);

int parse_init_open(struct parse *parse, const char *path);


