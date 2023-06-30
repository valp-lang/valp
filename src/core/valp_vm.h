#ifndef valp_vm_h
#define valp_vm_h

#include "valp_object.h"
#include "valp_value.h"
#include "valp_hash.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  valp_obj_closure *closure;
  uint8_t *ip;
  valp_value *slots;
} valp_call_frame;

typedef struct {
  valp_call_frame frames[FRAMES_MAX];
  int frame_count;

  valp_value stack[STACK_MAX];
  valp_value* stack_top;
  valp_hash globals;
  valp_hash constants;
  valp_hash strings;
  valp_string *init_string;
  valp_obj_upvalue *open_upvalues;

  size_t bytes_allocated;
  size_t next_gc;

  valp_obj *objects;
  int gray_count;
  int gray_capacity;
  valp_obj **gray_stack;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} valp_interpret_result;

extern VM vm;

void init_vm();
void free_vm();
valp_interpret_result interpret(const char *source);
void push(valp_value value);
valp_value pop();

#endif
