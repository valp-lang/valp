#include <stdio.h>
#include <string.h>

#include "valp_object.h"
#include "valp_memory.h"
#include "valp_value.h"

void init_valp_value_array(valp_value_array *array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void write_valp_value_array(valp_value_array *array, valp_value value) {
  if (array->capacity < array->count + 1) {
    int old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->values = GROW_ARRAY(valp_value, array->values, old_capacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void free_valp_value_array(valp_value_array *array) {
  FREE_ARRAY(valp_value, array->values, array->capacity);
  init_valp_value_array(array);
}

void print_value(valp_value value) {
  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL:    printf("nil"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJ:    print_object(value); break;
  }
}

bool values_equal(valp_value a, valp_value b) {
  if (a.type != b.type) return false;

  switch (a.type) {
    case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL:    return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_OBJ:    return AS_OBJ(a) == AS_OBJ(b);
    default: return false;
  }
}
