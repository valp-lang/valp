#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../include/valp.h"
#include "valp_vm.h"
#include "valp_debug.h"
#include "valp_compiler.h"
#include "valp_object.h"
#include "valp_memory.h"

VM vm;

static valp_value clock_native(int arg_count, valp_value *args) {
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void reset_stack() {
  vm.stack_top = vm.stack;
  vm.frame_count = 0;
  vm.open_upvalues = NULL;
}

static void runtime_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frame_count - 1; i >= 0; i--) {
    valp_call_frame* frame = &vm.frames[i];
    valp_function* function = frame->closure->function;
    // -1 because the IP is sitting on the next instruction to be
    // executed.
    size_t instruction = frame->ip - function->bytecode.code - 1;
    fprintf(stderr, "[line %d] in ", function->bytecode.lines[instruction]);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  reset_stack();
}

static void define_native(const char* name, valp_native_fn function) {
  push(OBJ_VAL(copy_string(name, (int)strlen(name))));
  push(OBJ_VAL(new_native(function)));
  hash_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void init_vm() {
  reset_stack();
  vm.objects = NULL;
  vm.bytes_allocated = 0;
  vm.next_gc = 1024 * 1024;

  vm.gray_count = 0;
  vm.gray_capacity = 0;
  vm.gray_stack = NULL;

  init_hash(&vm.globals);
  init_hash(&vm.strings);

  vm.init_string = NULL;
  vm.init_string = copy_string("init", 4);

  define_native("clock", clock_native);
}

void free_vm() {
  free_hash(&vm.globals);
  free_hash(&vm.strings);
  vm.init_string = NULL;
  free_objects();
}

void push(valp_value value) {
  *vm.stack_top = value;
  vm.stack_top++;
}

valp_value pop() {
  vm.stack_top--;
  return *vm.stack_top;
}

static valp_value peek(int distance) {
  return vm.stack_top[-1 - distance];
}

static bool call(valp_obj_closure *closure, int arg_count) {
  if (arg_count != closure->function->arity) {
    runtime_error("Expected %d arguments byt got %d.", closure->function->arity, arg_count);
    return false;
  }

  if (vm.frame_count == FRAMES_MAX) {
    runtime_error("Stack overflow.");
    return false;
  }

  valp_call_frame *frame = &vm.frames[vm.frame_count++];
  frame->closure = closure;
  frame->ip = closure->function->bytecode.code;

  frame->slots = vm.stack_top - arg_count - 1;
  return true;
}

static bool call_value(valp_value callee, int arg_count) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        valp_bound_method *bound = AS_BOUND_METHOD(callee);
        vm.stack_top[-arg_count - 1] = bound->receiver;
        return call(bound->method, arg_count);
      }
      case OBJ_CLASS: {
        valp_class *klass = AS_CLASS(callee);
        vm.stack_top[-arg_count - 1] = OBJ_VAL(new_instance(klass));

        valp_value initializer;
        if (hash_get(&klass->methods, vm.init_string, &initializer)) {
          return call(AS_CLOSURE(initializer), arg_count);
        } else if (arg_count != 0) {
          runtime_error("Expected 0 arguments but got %d.", arg_count);
          return false;
        }

        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), arg_count);
      case OBJ_NATIVE: {
        valp_native_fn native = AS_NATIVE(callee);
        valp_value result = native(arg_count, vm.stack_top - arg_count);
        vm.stack_top -= arg_count + 1;
        push(result);
        return true;
      }
      default:
        break;
    }
  }

  runtime_error("Can only call functions and classes");
  return false;
}

static bool invoke_from_class(valp_class *klass, valp_string *name, int arg_count) {
  valp_value method;
  if (!hash_get(&klass->methods, name, &method)) {
    runtime_error("Undefined property '%s'.", name->chars);
    return false;
  }

  return call(AS_CLOSURE(method), arg_count);
}

static bool invoke(valp_string *name, int arg_count) {
  valp_value receiver = peek(arg_count);

  if (!IS_INSTANCE(receiver)) {
    runtime_error("Only instances have methods.");
    return false;
  }

  valp_instance *instance = AS_INSTANCE(receiver);

  valp_value value;
  if (hash_get(&instance->fields, name, &value)) {
    vm.stack_top[-arg_count - 1] = value;
    return call_value(value, arg_count);
  }

  return invoke_from_class(instance->klass, name, arg_count);
}

static bool bind_method(valp_class *klass, valp_string *name) {
  valp_value method;

  if (!hash_get(&klass->methods, name, &method)) {
    runtime_error("Undefined property '%s'.", name->chars);
    return false;
  }

  valp_bound_method *bound = new_bound_method(peek(0), AS_CLOSURE(method));

  pop();
  push(OBJ_VAL(bound));
  return true;
}

static valp_obj_upvalue *capture_upvalue(valp_value *local) {
  valp_obj_upvalue *prev_upvalue = NULL;
  valp_obj_upvalue *upvalue = vm.open_upvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prev_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  valp_obj_upvalue *created_upvalue = new_upvalue(local);

  created_upvalue->next = upvalue;
  if (prev_upvalue == NULL) {
    vm.open_upvalues = created_upvalue;
  } else {
    prev_upvalue->next = created_upvalue;
  }

  return created_upvalue;
}

static void close_upvalues(valp_value *last) {
  while (vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
    valp_obj_upvalue *upvalue = vm.open_upvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.open_upvalues = upvalue->next;
  }
}

static void define_method(valp_string *name) {
  valp_value method = peek(0);
  valp_class *klass = AS_CLASS(peek(1));
  hash_set(&klass->methods, name, method);
  pop();
}

static bool is_falsey(valp_value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  valp_string *b = AS_STRING(peek(0));
  valp_string *a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  valp_string *result = take_string(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static valp_interpret_result run() {
  valp_call_frame *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->closure->function->bytecode.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_type, op) \
  do { \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtime_error("Operands must be numbers."); \
      return INTERPRET_RUNTIME_ERROR; \
    } \
    double b = AS_NUMBER(pop()); \
    double a = AS_NUMBER(pop()); \
    push(value_type(a op b)); \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
  printf("          ");
  for (valp_value *slot = vm.stack; slot < vm.stack_top; slot++) {
    printf("[ ");
    print_value(*slot);
    printf(" ]");
  }
  printf("\n");
  disassemble_instruction(&frame->closure->function->bytecode, (int)(frame->ip - frame->closure->function->bytecode.code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        valp_value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_NIL:       push(NIL_VAL); break;
      case OP_TRUE:      push(BOOL_VAL(true)); break;
      case OP_FALSE:     push(BOOL_VAL(false)); break;
      case OP_POP:       pop(); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        valp_string *name = READ_STRING();
        valp_value value;
        if (!hash_get(&vm.globals, name, &value)) {
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        valp_string *name = READ_STRING();
        hash_set(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        valp_string *name = READ_STRING();
        if (hash_set(&vm.globals, name, peek(0))) {
          hash_delete(&vm.globals, name);
          runtime_error("Undefined variable '%s'.", name->chars);
          return INTERPRET_COMPILE_ERROR;
        }
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          runtime_error("Only instances have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }

        valp_instance *instance = AS_INSTANCE(peek(0));
        valp_string *name = READ_STRING();

        valp_value value;
        if (hash_get(&instance->fields, name, &value)) {
          pop();
          push(value);
          break;
        }

        if (!bind_method(instance->klass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          runtime_error("Only instances have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }

        valp_instance *instance = AS_INSTANCE(peek(1));
        hash_set(&instance->fields, READ_STRING(), peek(0));

        valp_value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_GET_SUPER: {
        valp_string *name = READ_STRING();
        valp_class *superclass = AS_CLASS(pop());
        if (!bind_method(superclass, name)) { return INTERPRET_RUNTIME_ERROR; }

        break;
      }
      case OP_EQUAL: {
        valp_value b = pop();
        valp_value a = pop();
        push(BOOL_VAL(values_equal(a, b)));
        break;
      }
      case OP_GREATER:   BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:      BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtime_error("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT:  BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY:  BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:    BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:       push(BOOL_VAL(is_falsey(pop()))); break;
      case OP_NEGATE:
        if(!IS_NUMBER(peek(0))) {
          runtime_error("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }

        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      case OP_PRINT: {
        print_value(pop());
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        frame->ip += offset;
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (is_falsey(peek(0))) frame->ip += offset;
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }
      case OP_CALL: {
        int arg_count = READ_BYTE();
        if (!call_value(peek(arg_count), arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_INVOKE: {
        valp_string *method = READ_STRING();
        int arg_count = READ_BYTE();
        if (!invoke(method, arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_SUPER_INVOKE: {
        valp_string *method = READ_STRING();
        int arg_count = READ_BYTE();
        valp_class *superclass = AS_CLASS(pop());
        if (!invoke_from_class(superclass, method, arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_CLOSURE: {
        valp_function *function = AS_FUNCTION(READ_CONSTANT());
        valp_obj_closure *closure = new_closure(function);
        push(OBJ_VAL(closure));
        for (int i =0; i < closure->upvalue_count; i++) {
          uint8_t is_local = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (is_local) {
            closure->upvalues[i] = capture_upvalue(frame->slots + index);
          } else {
            closure->upvalues[i] = frame->closure->upvalues[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE: {
        close_upvalues(vm.stack_top - 1);
        pop();
        break;
      }
      case OP_RETURN: {
        valp_value result = pop();

        close_upvalues(frame->slots);

        vm.frame_count--;
        if (vm.frame_count == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stack_top = frame->slots;
        push(result);

        frame = &vm.frames[vm.frame_count - 1];
        break;
      }
      case OP_CLASS: {
        push(OBJ_VAL(new_class(READ_STRING())));
        break;
      }
      case OP_INHERIT: {
        valp_value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtime_error("Superclass muyst be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }
        valp_class *subclass = AS_CLASS(peek(0));
        hash_add_all(&AS_CLASS(superclass)->methods, &subclass->methods);
        pop();
        break;
      }
      case OP_METHOD: {
        define_method(READ_STRING());
        break;
      }
      case OP_DUP: { // bruhh xDDD
        valp_value b = pop();
        push(b);
        push(b);
      }
    }
  }

#undef BINARY_OP
#undef READ_STRING
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_BYTE
}

valp_interpret_result interpret(const char *source) {
  valp_function *function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  valp_obj_closure *closure = new_closure(function);
  pop();
  push(OBJ_VAL(closure));
  call_value(OBJ_VAL(closure), 0);

  return run();
}
