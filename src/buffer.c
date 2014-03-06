/* 2014, Copyright Intel & Jose Bollo <jose.bollo@open.eurogiciel.org>, license MIT */
#include <stdlib.h>
#include <errno.h>
#include <memory.h>
#include <limits.h>
#include <assert.h>

#include "buffer.h"

int buffer_set_count(struct buffer *buffer, int count)
{
	int result;
	int capacity;
	int *data;

	assert(buffer);
	assert(0 <= buffer->count);
	assert(0 <= count);

	if (count > buffer->capacity) {
		assert(buffer->capacity <= INT_MAX / 2);

		capacity = 2 * buffer->capacity;
		if (capacity < count)
			capacity = count;
		if (capacity < 256)
			capacity = 256;

		data = realloc(buffer->data, sizeof(int) * (size_t)capacity);
		if (!data)
			return -ENOMEM;

		buffer->capacity = capacity;
		buffer->data = data;
	}

	result = buffer->count;
	buffer->count = count;
	return result;
}

int buffer_alloc(struct buffer *buffer, int count)
{
	assert(buffer);
	assert(buffer->count <= INT_MAX - count);

	return buffer_set_count(buffer, buffer->count + count);
}

int buffer_strndup(struct buffer *buffer, const char *text, int length)
{
	int count, result;

	assert(buffer);
	assert(text);
	assert(0 <= length && length <= INT_MAX - sizeof(int));

	count = (length + sizeof(int)) / sizeof(int);
	assert(count > 0);
	result = buffer_alloc(buffer, count);
	if (result >= 0) {
		buffer->data[result + count - 1] = 0;
		memcpy(buffer->data + result, text, length);
	}

	return result;
}

int buffer_strdup(struct buffer *buffer, const char *text)
{
	size_t length;

	assert(buffer);
	assert(text);

	length = strlen(text);
	assert(length <= INT_MAX);

	return buffer_strndup(buffer, text, (int)length);
}

void buffer_init(struct buffer *buffer)
{
	assert(buffer);

	buffer->count = 0;
	buffer->capacity = 0;
	buffer->data = 0;
}

void buffer_reinit(struct buffer *buffer)
{
	assert(buffer);

	free(buffer->data);
	buffer_init(buffer);
}


