#include <stdlib.h>
#include <assert.h>
#include "vector.h"

#define VECTOR_INIT_SIZE 8

vector_t *make_vector(void)
{
    vector_t *vec = malloc(sizeof(*vec));
    vec->item = malloc(sizeof(void *) * VECTOR_INIT_SIZE);
    vec->top = 0;
    vec->size = VECTOR_INIT_SIZE;
    return vec;
}

void vector_append(vector_t *vec, void *val)
{
    assert(vec);
    if (vec->top == vec->size) {
        vec->size += vec->size >> 1;
        vec->item = realloc(vec->item, vec->size);
    }
    vec->item[vec->top++] = val;
}

void *vector_get(vector_t *vec, size_t idx)
{
    assert(vec && vec->top > idx);
    return vec->item[idx];
}

void free_vector(vector_t *vec, void (*free_item)(void *))
{
    size_t i;

    assert(vec);
    if (free_item)
        for (i = 0; i < vector_len(vec); i++)
            (*free_item)(vec->item[i]);
    free(vec->item);
    free(vec);
}
