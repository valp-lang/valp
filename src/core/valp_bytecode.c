#include <stdlib.h>

#include "valp_bytecode.h"
#include "valp_memory.h"
#include "valp_vm.h"

void init_bytecode(valp_bytecode *bytecode) {
  bytecode->count = 0;
  bytecode->capacity = 0;
  bytecode->code = NULL;
  bytecode->lines = NULL;
  init_valp_value_array(&bytecode->constants);
}

void free_bytecode(valp_bytecode *bytecode) {
  FREE_ARRAY(uint8_t, bytecode->code, bytecode->capacity);
  FREE_ARRAY(int, bytecode->lines, bytecode->capacity);
  free_valp_value_array(&bytecode->constants);
  init_bytecode(bytecode);
}

void write_bytecode(valp_bytecode *bytecode, uint8_t byte, int line) {
  if (bytecode->capacity < bytecode->count + 1) {
    int old_capacity = bytecode->capacity;
    bytecode->capacity = GROW_CAPACITY(old_capacity);
    bytecode->code = GROW_ARRAY(uint8_t, bytecode->code, old_capacity, bytecode->capacity);
    bytecode->lines = GROW_ARRAY(int, bytecode->lines, old_capacity, bytecode->capacity);
  }

  bytecode->code[bytecode->count] = byte;
  bytecode->lines[bytecode->count] = line;
  bytecode->count++;
}

int add_constant(valp_bytecode *bytecode, valp_value value) {
  push(value);
  write_valp_value_array(&bytecode->constants, value);
  pop();
  return bytecode->constants.count - 1;
}
