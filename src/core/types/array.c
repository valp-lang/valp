#include <stdio.h>

#include "array.h"
#include "../valp_native.h"
#include "../valp_vm.h"

static valp_value array_length(int arg_count, valp_value *args) {
  if (arg_count != 0) { 
    runtime_error("len() takes 0 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_array *arr = AS_ARRAY(args[0]);
  return NUMBER_VAL(arr->values.count);
}

static valp_value array_push(int arg_count, valp_value *args) {
  if (arg_count != 1) { 
    runtime_error("push() takes 1 argument, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_array *arr = AS_ARRAY(args[0]);
  valp_value element = args[1];

  write_valp_value_array(&arr->values, element);

  return NIL_VAL;
}

static valp_value array_pop(int arg_count, valp_value *args) {
  if (arg_count != 0) { 
    runtime_error("pop() takes 0 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_array *arr = AS_ARRAY(args[0]);

  if (arr->values.count == 0) { return NIL_VAL; }

  valp_value last_element = arr->values.values[arr->values.count - 1];
  arr->values.count--;

  return last_element;
}

static valp_value array_insert(int arg_count, valp_value *args) {
  if (arg_count != 1) { 
    runtime_error("insert() takes 0 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_array *arr = AS_ARRAY(args[0]);
  valp_value element = args[1];

  prepend_valp_value_array(&arr->values, element);

  return NIL_VAL;
}

static valp_value array_drop(int arg_count, valp_value *args) {
  if (arg_count != 0) { 
    runtime_error("drop() takes 0 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_array *arr = AS_ARRAY(args[0]);

  if (arr->values.count == 0) { return NIL_VAL; }

  valp_value first_element = arr->values.values[0];

  for (int i = 0; i < arr->values.count - 1; i++) {
    arr->values.values[i] = arr->values.values[i + 1];
  }

  arr->values.count--;

  return first_element;
}

static valp_value array_empty(int arg_count, valp_value *args) {
  if (arg_count != 0) { 
    runtime_error("empty?() takes 1 argument, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_array *arr = AS_ARRAY(args[0]);

  return BOOL_VAL(arr->values.count == 0);
}

void define_array_methods() {
  define_native(&vm.array_methods, "len", array_length);
  define_native(&vm.array_methods, "push", array_push);
  define_native(&vm.array_methods, "pop", array_pop);
  define_native(&vm.array_methods, "insert", array_insert);
  define_native(&vm.array_methods, "drop", array_drop);
  define_native(&vm.array_methods, "is_empty", array_empty);
}