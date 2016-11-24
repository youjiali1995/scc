#ifndef BUFFER_H__
#define BUFFER_H__

#include <stddef.h>

typedef struct buffer_t {
    char *stack;
    size_t size;
    size_t top;
} buffer_t;

buffer_t *make_buffer(void);
void buffer_push(buffer_t *buffer, const void *v, size_t size);
void *buffer_pop(buffer_t *buffer, size_t size);
void buffer_free(buffer_t *buffer);

#endif
