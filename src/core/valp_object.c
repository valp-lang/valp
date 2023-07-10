#include <stdio.h>
#include <string.h>

#include "valp_memory.h"
#include "valp_object.h"
#include "valp_value.h"
#include "valp_vm.h"
#include "valp_hash.h"

#define ALLOCATE_OBJ(type, object_type) (type*)allocate_object(sizeof(type), object_type)

static valp_obj *allocate_object(size_t size, valp_obj_type type) {
  valp_obj *object = (valp_obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->is_marked = false;
  
  object->next = vm.objects;
  vm.objects = object;

#ifdef DEBUG_LOG_GC
  printf("%p allocate %ld for %d\n", (void*)object, size, type);
#endif

  return object;
}

valp_bound_method *new_bound_method(valp_value receiver, valp_obj_closure *method) {
  valp_bound_method *bound = ALLOCATE_OBJ(valp_bound_method, OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

valp_class *new_class(valp_string *name) {
  valp_class *klass = ALLOCATE_OBJ(valp_class, OBJ_CLASS);
  klass->name = name;
  init_hash(&klass->methods);
  return klass;
}

valp_obj_closure *new_closure(valp_function *function) {
  valp_obj_upvalue **upvalues = ALLOCATE(valp_obj_upvalue*, function->upvalue_count);

  for (int i = 0; i < function->upvalue_count; i++) {
    upvalues[i] = NULL;
  }

  valp_obj_closure *closure = ALLOCATE_OBJ(valp_obj_closure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

valp_function *new_function() {
  valp_function *function = ALLOCATE_OBJ(valp_function, OBJ_FUNCTION);

  function->arity = 0;
  function->upvalue_count = 0;
  function->name = NULL;
  init_bytecode(&function->bytecode);

  return function;
}

valp_instance *new_instance(valp_class *klass) {
  valp_instance *instance = ALLOCATE_OBJ(valp_instance, OBJ_INSTANCE);
  instance->klass = klass;
  init_hash(&instance->fields);
  return instance;
}

valp_obj_native *new_native(valp_native_fn function) {
  valp_obj_native *native = ALLOCATE_OBJ(valp_obj_native, OBJ_NATIVE);
  native->function = function;
  
  return native;
}

static valp_string *allocate_string(char *chars, int length, uint32_t hash) {
  valp_string *string = ALLOCATE_OBJ(valp_string, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  push(OBJ_VAL(string));
  hash_set(&vm.strings, string, NIL_VAL);
  pop();

  return string;
}

static uint32_t hash_string(const char* key, int length) {
  uint32_t hash = 2166136261u;

  for (int i = 0; i < length; i++) {
    hash ^= key[i];
    hash *= 16777619;
  }

  return hash;
}

valp_string *take_string(char *chars, int length) {
  uint32_t hash = hash_string(chars, length);
  valp_string *interned = hash_find_string(&vm.strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocate_string(chars, length, hash);
}

valp_string *copy_string(const char *chars, int length) {
  uint32_t hash = hash_string(chars, length);
  valp_string *interned = hash_find_string(&vm.strings, chars, length, hash);

  if (interned != NULL) return interned;

  char *heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';

  return allocate_string(heap_chars, length, hash);
}

valp_obj_upvalue *new_upvalue(valp_value *slot) {
  valp_obj_upvalue *upvalue = ALLOCATE_OBJ(valp_obj_upvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

valp_array *new_array() {
  valp_array *array = ALLOCATE_OBJ(valp_array, OBJ_ARRAY);
  init_valp_value_array(&array->values);

  return array;
}

static void print_function(valp_function *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", function->name->chars);
}

static void print_array(valp_array *arr) {
  printf("[");

  for (int i = 0; i < arr->values.count; ++i) {
    valp_value element = arr->values.values[i];

    if (IS_STRING(element)) {
      printf("%s", AS_STRING(element)->chars);
    } else {
      print_value(element);
    }

    if (i != arr->values.count - 1) { printf(", "); }
  }

  printf("]");
}

void print_object(valp_value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD: print_function(AS_BOUND_METHOD(value)->method->function); break;
    case OBJ_CLASS:        printf("%s", AS_CLASS(value)->name->chars); break;
    case OBJ_CLOSURE:      print_function(AS_CLOSURE(value)->function); break;
    case OBJ_FUNCTION:     print_function(AS_FUNCTION(value)); break;
    case OBJ_INSTANCE:     printf("%s instance", AS_INSTANCE(value)->klass->name->chars); break;
    case OBJ_NATIVE:       printf("<native fn>"); break;
    case OBJ_STRING:       printf("%s", AS_CSTRING(value)); break;
    case OBJ_UPVALUE:      printf("upvalue"); break;
    case OBJ_ARRAY:        print_array(AS_ARRAY(value)); break;
  }
}