/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */

/*
 * The buffer structure is used to have compiled version
 * of files and improve the upload speed.
 */
struct buffer {
	int count;		/* count of data used */
	int capacity;	/* count of data allocated */
	int *data;		/* the data */
};

/* Set (may allocate) a new count of data and returns the previous count or error */
int buffer_set_count(struct buffer *buffer, int count);

/* Allocates count integers in buffer and returns the index */
int buffer_alloc(struct buffer *buffer, int count);

/* Allocates and copy a string of text and length in buffer */
int buffer_strndup(struct buffer *buffer, const char *text, int length);

/* Allocates and copy a string of text buffer */
int buffer_strdup(struct buffer *buffer, const char *text);

/* Initial init of the buffer */
void buffer_init(struct buffer *buffer);

/* Free the memory and init */
void buffer_reinit(struct buffer *buffer);


