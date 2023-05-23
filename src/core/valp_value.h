#ifndef valp_value_h
#define valp_value_h

#include "../include/valp.h"

typedef struct valp_obj valp_obj;
typedef struct valp_string valp_string;

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJ
} valp_value_type;

typedef struct {
  valp_value_type type;
  union {
    bool boolean;
    double number;
    valp_obj* valp_obj;
  } as;
} valp_value;

#define IS_BOOL(value)        ((value).type == VAL_BOOL)
#define IS_NIL(value)         ((value).type == VAL_NIL)
#define IS_NUMBER(value)      ((value).type == VAL_NUMBER)
#define IS_OBJ(value)         ((value).type == VAL_OBJ)

#define AS_BOOL(value)        ((value).as.boolean)
#define AS_NUMBER(value)      ((value).as.number)
#define AS_OBJ(value)         ((value).as.valp_obj)

#define BOOL_VAL(value)       ((valp_value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL               ((valp_value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value)     ((valp_value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)       ((valp_value){VAL_OBJ, {.valp_obj = (valp_obj*)object}})

typedef struct {
  int capacity;
  int count;
  valp_value *values;
} valp_value_array;

bool values_equal(valp_value a, valp_value b);
void init_valp_value_array(valp_value_array *array);
void write_valp_value_array(valp_value_array *array, valp_value value);
void free_valp_value_array(valp_value_array *array);
void print_value(valp_value value);

#endif
