#include <time.h>

#include "../include/valp.h"
#include "valp_native.h"
#include "valp_object.h"
#include "valp_vm.h"

static valp_value clock_native(int arg_count, valp_value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static valp_value assert_native(int arg_count, valp_value *args) {
  if (arg_count != 1) {
    runtime_error("assert() expected 1 argument, got %d.", arg_count);
    return UNDEFINED_VAL;
  }

  if (is_falsey(args[0])) {
    runtime_error("Assertion failed.");
    return UNDEFINED_VAL;
  }

  return NIL_VAL;
}

static valp_value assert_equal_native(int arg_count, valp_value *args) {
  if (arg_count != 2) {
    runtime_error("assert_equal() expected 2 arguments, got %d.", arg_count);
    return UNDEFINED_VAL;
  }

  if (!values_equal(args[0], args[1])) {
    runtime_error("Assertion failed.");
    return UNDEFINED_VAL;
  }

  return NIL_VAL;
}

void define_native(valp_hash *hash, const char* name, valp_native_fn function) {
  push(OBJ_VAL(copy_string(name, (int)strlen(name))));
  push(OBJ_VAL(new_native(function)));

  hash_set(hash, AS_STRING(vm.stack[0]), vm.stack[1]);

  pop();
  pop();
}

void define_natives() {
  char *natives[] = { "clock", "assert", "assert_equal" };

  valp_native_fn natives_f[] = { clock_native, assert_native, assert_equal_native };

  for (int i = 0; i < sizeof(natives) / sizeof(natives[0]); ++i) {
    define_native(&vm.globals, natives[i], natives_f[i]);
  }
}