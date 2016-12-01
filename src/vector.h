#ifndef VECTOR_H__
#define VECTOR_H__

#include <stddef.h>

typedef struct vetcor_t {
    void **item;
    size_t top;
    size_t size;
} vector_t;

#define vector_len(v) ((v) ?(v)->top : 0)

vector_t *make_vector(void);
void vector_append(vector_t *v, void *val);
void *vector_get(vector_t *v, size_t idx);
void free_vector(vector_t *v, void (*free_item)(void *));

#endif
