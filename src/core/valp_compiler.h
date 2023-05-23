#ifndef valp_compiler_h
#define valp_compiler_h

#include "valp_vm.h"
#include "valp_object.h"

valp_function *compile(const char *source);
void mark_compiler_roots();

#endif
