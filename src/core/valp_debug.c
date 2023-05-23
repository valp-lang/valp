#include <stdio.h>

#include "valp_debug.h"
#include "valp_object.h"
#include "valp_value.h"

void disassemble_bytecode(valp_bytecode *bytecode, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < bytecode->count;) {
    offset = disassemble_instruction(bytecode, offset);
  }
}

static int constant_instruction(const char *name, valp_bytecode *bytecode, int offset) {
  uint8_t constant = bytecode->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  print_value(bytecode->constants.values[constant]);
  printf("\n");

  return offset + 2;
}

static int invoke_instruction(const char* name, valp_bytecode* bytecode, int offset) {
  uint8_t constant = bytecode->code[offset + 1];
  uint8_t arg_count = bytecode->code[offset + 2];
  printf("%-16s (%d args) %4d '", name, arg_count, constant);
  print_value(bytecode->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}


static int simple_instruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byte_instruction(const char *name, valp_bytecode *bytecode, int offset) {
  uint8_t slot = bytecode->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jump_instruction(const char *name, int sign, valp_bytecode *bytecode, int offset) {
  uint16_t jump = (uint16_t)(bytecode->code[offset + 1] << 8);
  jump |= bytecode->code[offset + 2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 * sign * jump);
  
  return offset + 3;
}

int disassemble_instruction(valp_bytecode *bytecode, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && bytecode->lines[offset] == bytecode->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", bytecode->lines[offset]);
  }

  uint8_t instruction = bytecode->code[offset];
  switch (instruction) {
    case OP_CONSTANT:       return constant_instruction("OP_CONSTANT", bytecode, offset);
    case OP_NIL:            return simple_instruction("OP_NIL", offset);
    case OP_TRUE:           return simple_instruction("OP_TRUE", offset);
    case OP_FALSE:          return simple_instruction("OP_FALSE", offset);
    case OP_POP:            return simple_instruction("OP_POP", offset);
    case OP_GET_LOCAL:      return byte_instruction("OP_GET_LOCAL", bytecode, offset);
    case OP_SET_LOCAL:      return byte_instruction("OP_SET_LOCAL", bytecode, offset);
    case OP_GET_GLOBAL:     return constant_instruction("OP_GET_GLOBAL", bytecode, offset);
    case OP_DEFINE_GLOBAL:  return constant_instruction("OP_DEFINE_GLOBAL", bytecode, offset);
    case OP_SET_GLOBAL:     return constant_instruction("OP_SET_GLOBAL", bytecode, offset);
    case OP_GET_UPVALUE:    return byte_instruction("OP_GET_UPVALUE", bytecode, offset);
    case OP_SET_UPVALUE:    return byte_instruction("OP_SET_UPVALUE", bytecode, offset);
    case OP_GET_PROPERTY:   return constant_instruction("OP_GET_PROPERTY", bytecode, offset);
    case OP_SET_PROPERTY:   return constant_instruction("OP_SET_PROPERTY", bytecode, offset);
    case OP_GET_SUPER:      return constant_instruction("OP_GET_SUPER", bytecode, offset);
    case OP_EQUAL:          return simple_instruction("OP_EQUAL", offset);
    case OP_GREATER:        return simple_instruction("OP_GREATER", offset);
    case OP_LESS:           return simple_instruction("OP_LESS", offset);
    case OP_ADD:            return simple_instruction("OP_ADD", offset);
    case OP_SUBTRACT:       return simple_instruction("OP_SUBTRACT", offset);
    case OP_MULTIPLY:       return simple_instruction("OP_MULTIPLY", offset);
    case OP_DIVIDE:         return simple_instruction("OP_DIVIDE", offset);
    case OP_NOT:            return simple_instruction("OP_NOT", offset);
    case OP_NEGATE:         return simple_instruction("OP_NEGATE", offset);
    case OP_PRINT:          return simple_instruction("OP_PRINT", offset);
    case OP_JUMP:           return jump_instruction("OP_JUMP", 1, bytecode, offset);
    case OP_JUMP_IF_FALSE:  return jump_instruction("OP_JUMP_IF_FALSE", 1, bytecode, offset);
    case OP_LOOP:           return jump_instruction("OP_LOOP", -1, bytecode, offset);
    case OP_CALL:           return byte_instruction("OP_CALL", bytecode, offset);
    case OP_INVOKE:         return invoke_instruction("OP_INVOKE", bytecode, offset);
    case OP_SUPER_INVOKE:   return invoke_instruction("OP_SUPER_INVOKE", bytecode, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant = bytecode->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      print_value(bytecode->constants.values[constant]);
      printf("\n");

      valp_function *function = AS_FUNCTION(bytecode->constants.values[constant]);
      for (int j = 0; j < function->upvalue_count; j++) {
        int is_local = bytecode->code[offset++];
        int index = bytecode->code[offset++];
        printf("%04d      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
      }

      return offset;
    }
    case OP_CLOSE_UPVALUE:  return simple_instruction("OP_CLOSE_UPVALUE", offset);
    case OP_RETURN:         return simple_instruction("OP_RETURN", offset);
    case OP_CLASS:          return constant_instruction("OP_CLASS", bytecode, offset);
    case OP_INHERIT:        return simple_instruction("OP_INHERIT", offset);
    case OP_METHOD:         return constant_instruction("OP_METHOD", bytecode, offset);
    default:
      printf("Unknown opcode %d\n", instruction);
      return offset + 1;
  }
}
