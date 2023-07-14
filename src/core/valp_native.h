#ifndef valp_native_h
#define valp_native_h

#include "valp_hash.h"
#include "valp_object.h"

void define_natives();
void define_native(valp_hash *hash, const char* name, valp_native_fn function);

#endif