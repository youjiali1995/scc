#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "dict.h"

static size_t hash(const char *s)
{
    size_t r = 2166136261;
    for (; *s; s++) {
        r ^= *s;
        r *= 16777619;
    }
    return r;
}

#define DICT_INIT_SIZE 8

dict_t *make_dict(dict_t *link)
{
    dict_t *dict = malloc(sizeof(*dict));
    dict->link = link;
    dict->used = 0;
    dict->mask = DICT_INIT_SIZE - 1;
    dict->table = calloc(DICT_INIT_SIZE, sizeof(dict_entry_t));
    return dict;
}

#define PROBE(i) ((i) + 1)

static dict_entry_t *lookup(dict_t *dict, const char *key, size_t h)
{
    size_t i;
    dict_entry_t *e;

    for (i = h & dict->mask; ; i = PROBE(i) & dict->mask) {
        e = &dict->table[i];
        if (!e->key || (e->hash == h && (e->key == key || !strcmp(e->key, key))))
            return e;
    }
}

void *dict_lookup(dict_t *dict, const char *key)
{
    dict_entry_t *e;

    assert(dict && key);
    while (dict) {
        e = lookup(dict, key, hash(key));
        if (e->key)
            return e->val;
        dict = dict->link;
    }
    return NULL;
}

static void dict_resize(dict_t *dict, size_t new_size)
{
    dict_entry_t *e, *old = dict->table;
    dict->table = calloc(new_size, sizeof(dict_entry_t));
    dict->mask = new_size - 1;
    size_t i = dict->used;
    dict->used = 0;

    for (e = old; i > 0; e++) {
        if (e->key) {
            dict_insert(dict, e->key, e->val, 0);
            i--;
        }
    }
    free(old);
}

bool dict_insert(dict_t *dict, char *key, void *val, bool flag)
{
    dict_entry_t *e;
    size_t h;

    assert(dict && key);
    h = hash(key);
    e = lookup(dict, key, h);
    if (!e->key) {
        e->hash = h;
        e->key = key;
        e->val = val;
        dict->used++;
    } else {
        if (flag)
            return false;
        else
            e->val = val;
    }

    if (dict->used * 3 >= (dict->mask + 1) * 2)
        dict_resize(dict, (dict->mask + 1) * 2);
    return true;
}

void free_dict(dict_t *dict, void (*free_key)(char *), void (*free_val)(void *))
{
    dict_entry_t *e;

    assert(dict);
    for (e = dict->table; dict->used > 0; e++) {
        if (e->key) {
            if (free_key)
                (*free_key)(e->key);
            if (free_val)
                (*free_val)(e->val);
            dict->used--;
        }
    }
    free(dict->table);
    free(dict);
}
