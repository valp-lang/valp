#ifndef valp_debug_h
#define valp_debug_h

#include "valp_bytecode.h"

void disassemble_bytecode(valp_bytecode *bytecode, const char *name);
int disassemble_instruction(valp_bytecode *bytecode, int offset);

#endif
