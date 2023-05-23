#ifndef valp_hash_h
#define valp_hash_h

#include "../include/valp.h"
#include "valp_value.h"

typedef struct {
  valp_string *key;
  valp_value value;
} valp_entry;

typedef struct {
  int count;
  int capacity;
  valp_entry *entries;
} valp_hash;

void init_hash(valp_hash *hash);
void free_hash(valp_hash *hash);
bool hash_get(valp_hash *hash, valp_string *key, valp_value *value);
bool hash_set(valp_hash *hash, valp_string *key, valp_value value);
bool hash_delete(valp_hash *hash, valp_string *key);
void hash_add_all(valp_hash *from, valp_hash *to);
valp_string *hash_find_string(valp_hash *hash, const char *chars, int length, uint32_t string_hash);
void hash_remove_white(valp_hash *hash);
void mark_hash(valp_hash *hash);

#endif