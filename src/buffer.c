#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "buffer.h"

#define BUFFER_INIT_SIZE 32

buffer_t *make_buffer(void)
{
    return calloc(1, sizeof(buffer_t));
}

void buffer_push(buffer_t *buffer, const void *v, size_t size)
{
    assert(buffer && size >= 0);
    if (buffer->top + size > buffer->size) {
        if (buffer->size == 0)
            buffer->size = BUFFER_INIT_SIZE;
        while (buffer->top + size > buffer->size)
            buffer->size += buffer->size >> 1;
        buffer->stack = realloc(buffer->stack, buffer->size);
    }
    memcpy(buffer->stack + buffer->top, v, size);
    buffer->top += size;
}

void *buffer_pop(buffer_t *buffer, size_t size)
{
    assert(buffer && buffer->top >= size);
    buffer->top -= size;
    return buffer->stack + buffer->top;
}

void buffer_free(buffer_t *buffer)
{
    if (buffer) {
        free(buffer->stack);
        free(buffer);
    }
}
