#ifndef valp_bytecode_h
#define valp_bytecode_h

#include "../include/valp.h"
#include "valp_value.h"

typedef enum {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_UPVALUE,
  OP_SET_UPVALUE,
  OP_GET_PROPERTY,
  OP_GET_PROPERTY_NO_POP,
  OP_SET_PROPERTY,
  OP_GET_SUPER,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_JUMP_COMPARE,
  OP_LOOP,
  OP_CALL,
  OP_INVOKE,
  OP_SUPER_INVOKE,
  OP_CLOSURE,
  OP_CLOSE_UPVALUE,
  OP_RETURN,
  OP_CLASS,
  OP_INHERIT,
  OP_METHOD,
  OP_DUP,
  OP_NEW_ARRAY,
  OP_SLICE,
  OP_BREAK,
} valp_op_code;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  int *lines;
  valp_value_array constants;
} valp_bytecode;

void init_bytecode(valp_bytecode *bytecode);
void free_bytecode(valp_bytecode *bytecode);
void write_bytecode(valp_bytecode *bytecode, uint8_t byte, int line);
int add_constant(valp_bytecode *bytecode, valp_value value);

#endif
