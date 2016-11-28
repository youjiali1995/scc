#ifndef DICT_H__
#define DICT_H__

#include <stddef.h> /* for size_t */
#include <stdbool.h>

typedef struct dict_entry_t {
    size_t hash;
    char *key;
    void *val;
} dict_entry_t;

typedef struct dict_t {
    struct dict_t *link;
    size_t used;
    size_t mask;
    dict_entry_t *table;
} dict_t;

dict_t *make_dict(dict_t *link);
void *dict_lookup(dict_t *dict, const char *key);
/* flag == true: insert if key is not in dict, if key is in dict, return false
 * flag == false: insert no matter whether key is in dict
 * succecc return true
 */
bool dict_insert(dict_t *dict, char *key, void *val, bool flag);
void free_dict(dict_t *dict, void (*free_key)(char *), void (*free_val)(void *));

#endif
