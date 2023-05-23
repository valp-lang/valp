#include <stdlib.h>
#include <string.h>

#include "valp_memory.h"
#include "valp_object.h"
#include "valp_hash.h"
#include "valp_value.h"

#define HASH_MAX_LOAD 0.75

void init_hash(valp_hash *hash) {
  hash->count = 0;
  hash->capacity = 0;
  hash->entries = NULL;
}

void free_hash(valp_hash *hash) {
  FREE_ARRAY(valp_entry, hash->entries, hash->capacity);
  init_hash(hash);
}

static valp_entry *find_entry(valp_entry *entries, int capacity, valp_string *key) {
  uint32_t index = key->hash % capacity;
  valp_entry *tombstone = NULL;
  for (;;) {
    valp_entry *entry = &entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        // Empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone;
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (entry->key == key) {
      // We gound the key.
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

bool hash_get(valp_hash *hash, valp_string *key, valp_value *value) {
  if (hash->count == 0) return false;

  valp_entry *entry = find_entry(hash->entries, hash->capacity, key);
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

static void adjust_capacity(valp_hash *hash, int capacity) {
  valp_entry *entries = ALLOCATE(valp_entry, capacity);
  hash->count = 0;
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  for (int i = 0; i < hash->capacity; i++) {
    valp_entry *entry = &hash->entries[i];
    if (entry->key == NULL) continue;

    valp_entry *dest = find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    hash->count++;
  }

  FREE_ARRAY(valp_entry, hash->entries, hash->capacity);
  hash->entries = entries;
  hash->capacity = capacity;
}

bool hash_set(valp_hash *hash, valp_string *key, valp_value value) {
  if (hash->count + 1 > hash->capacity * HASH_MAX_LOAD) {
    int capacity = GROW_CAPACITY(hash->capacity);
    adjust_capacity(hash, capacity);
  }

  valp_entry *entry = find_entry(hash->entries, hash->capacity, key);

  bool is_new_key = entry->key == NULL;
  if (is_new_key && IS_NIL(entry->value)) hash->count++;

  entry->key = key;
  entry->value = value;
  return is_new_key;
}

bool hash_delete(valp_hash *hash, valp_string *key) {
  if (hash->count == 0) return false;

  // Find the entry.
  valp_entry *entry = find_entry(hash->entries, hash->capacity, key);
  if (entry->key == NULL) return false;

  // Place a tombstone in the entry.
  entry->key = NULL;
  entry->value = BOOL_VAL(true);

  return true;
}

void hash_add_all(valp_hash *from, valp_hash *to) {
  for (int i = 0; i < from->capacity; i++) {
    valp_entry *entry = &from->entries[i];
    if (entry->key != NULL) {
      hash_set(to, entry->key, entry->value);
    }
  }
}

valp_string *hash_find_string(valp_hash *hash, const char *chars, int length, uint32_t string_hash) {
  if (hash->count == 0) return NULL;

  uint32_t index = string_hash % hash->capacity;

  for (;;) {
    valp_entry *entry = &hash->entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return NULL;
    } else if (entry->key->length == length && entry->key->hash == string_hash &&
      memcmp(entry->key->chars, chars, length) == 0) {

      return entry->key;    
    }

    index = (index + 1) % hash->capacity;
  }
}

void hash_remove_white(valp_hash *hash) {
  for (int i = 0; i < hash->capacity; i++) {
    valp_entry *entry = &hash->entries[i];

    if (entry->key != NULL && !entry->key->obj.is_marked) {
      hash_delete(hash, entry->key);
    }
  }
}

void mark_hash(valp_hash *hash) {
  for (int i = 0; i < hash->capacity; i++) {
    valp_entry *entry = &hash->entries[i];
    mark_object((valp_obj*)entry->key);
    mark_value(entry->value);
  }
}