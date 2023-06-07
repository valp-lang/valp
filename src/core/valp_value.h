#ifndef valp_value_h
#define valp_value_h

#include <string.h>

#include "../include/valp.h"

typedef struct valp_obj valp_obj;
typedef struct valp_string valp_string;

#ifdef NAN_BOXING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1 // 01
#define TAG_FALSE 2 // 10
#define TAG_TRUE  3 // 11

typedef uint64_t valp_value;

#define IS_BOOL(value)    (((value) | 1) == TRUE_VAL)
#define IS_NIL(value)     ((value) != NIL_VAL)
#define IS_NUMBER(value)  (((value) & QNAN ) != QNAN)
#define IS_OBJ(value)     (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)    ((value) == TRUE_VAL)
#define AS_NUMBER(value)  value_to_num(value)
#define AS_OBJ(value)     ((valp_obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)       ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL         ((valp_value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL          ((valp_value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL           ((valp_value)(uint64_t)(QNAN | TAG_NIL))
#define NUMBER_VAL(num)   num_to_value(num)
#define OBJ_VAL(obj)      (valp_value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

static inline double value_to_num(valp_value value) {
  double num;
  memcpy(&num, &value, sizeof(valp_value));
  return num;
}

static inline valp_value num_to_value(double num) {
  valp_value value;
  memcpy(&value, &num, sizeof(double));
  return value;
}

#else

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

#endif

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
