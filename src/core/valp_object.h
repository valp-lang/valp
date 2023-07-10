#ifndef valp_object_h
#define valp_object_h

#include "../include/valp.h"
#include "valp_value.h"
#include "valp_bytecode.h"
#include "valp_hash.h"

#define OBJ_TYPE(value)    (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value) is_obj_type(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        is_obj_type(value, OBJ_CLASS)
#define IS_CLOSURE(value)      is_obj_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     is_obj_type(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     is_obj_type(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       is_obj_type(value, OBJ_NATIVE)
#define IS_STRING(value)       is_obj_type(value, OBJ_STRING)
#define IS_ARRAY(value)        is_obj_type(value, OBJ_ARRAY)

#define AS_BOUND_METHOD(value) ((valp_bound_method*)AS_OBJ(value))
#define AS_CLASS(value)        ((valp_class*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((valp_obj_closure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((valp_function*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((valp_instance*)AS_OBJ(value))
#define AS_NATIVE(value)       (((valp_obj_native*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((valp_string*)AS_OBJ(value))
#define AS_CSTRING(value)      (((valp_string*)AS_OBJ(value))->chars)
#define AS_ARRAY(value)        ((valp_array*)AS_OBJ(value))

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE,
  OBJ_ARRAY,
} valp_obj_type;

struct valp_obj {
  valp_obj_type type;
  bool is_marked;
  struct valp_obj* next;
};

typedef struct {
  valp_obj obj;
  int arity;
  int upvalue_count;
  valp_bytecode bytecode;
  valp_string *name;
} valp_function;

typedef valp_value (*valp_native_fn)(int arg_count, valp_value *args);

typedef struct {
  valp_obj obj;
  valp_native_fn function;
} valp_obj_native;

struct valp_string {
  valp_obj obj;
  int length;
  char *chars;
  uint32_t hash;
};

typedef struct {
  valp_obj obj;
  valp_value_array values;
} valp_array;

typedef struct valp_obj_upvalue {
  valp_obj obj;
  valp_value *location;
  valp_value closed;
  struct valp_obj_upvalue *next;
} valp_obj_upvalue;

typedef struct {
  valp_obj obj;
  valp_function *function;
  valp_obj_upvalue **upvalues;
  int upvalue_count;
} valp_obj_closure;

typedef struct {
  valp_obj obj;
  valp_string *name;
  valp_hash methods;
} valp_class;

typedef struct {
  valp_obj obj;
  valp_class *klass;
  valp_hash fields;
} valp_instance;

typedef struct {
  valp_obj obj;
  valp_value receiver;
  valp_obj_closure *method;
} valp_bound_method;

valp_bound_method *new_bound_method(valp_value receiver, valp_obj_closure *method);
valp_class *new_class(valp_string *name);
valp_obj_closure *new_closure(valp_function *function);
valp_function *new_function();
valp_instance *new_instance(valp_class *klass);
valp_obj_native *new_native(valp_native_fn function);
valp_string *take_string(char *chars, int length);
valp_string *copy_string(const char *chars, int length);
valp_obj_upvalue *new_upvalue(valp_value *slot);
valp_array *new_array();
void print_object(valp_value value);

static inline bool is_obj_type(valp_value value, valp_obj_type type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif