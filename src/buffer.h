/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
struct buffer {
	int count;
	int capacity;
	int *data;
};


/* set (may allocate) a new count of data and returns the previous count 
or error */
int buffer_set_count(struct buffer *buffer, int count);

/* allocates count integers in buffer and returns the index */
int buffer_alloc(struct buffer *buffer, int count);

/* allocates and copy a string of text and length in buffer */
int buffer_strndup(struct buffer *buffer, const char *text, int length);

/* allocates and copy a string of text buffer */
int buffer_strdup(struct buffer *buffer, const char *text);

/* initial init of the buffer */
void buffer_init(struct buffer *buffer);

/* free the memory and init */
void buffer_reinit(struct buffer *buffer);


