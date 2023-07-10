#include <time.h>

#include "valp_native.h"
#include "../include/valp.h"
#include "valp_object.h"
#include "valp_vm.h"

static valp_value clock_native(int arg_count, valp_value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void define_native(const char* name, valp_native_fn function) {
  push(OBJ_VAL(copy_string(name, (int)strlen(name))));
  push(OBJ_VAL(new_native(function)));

  hash_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);

  pop();
  pop();
}

void define_natives() {
  char *natives[] = { "clock" };

  valp_native_fn natives_f[] = { clock_native };

  for (int i = 0; i < sizeof(natives) / sizeof(natives[0]); ++i) {
    define_native(natives[i], natives_f[i]);
  }
}