#include <stdlib.h>

#include "valp_compiler.h"
#include "valp_memory.h"
#include "valp_vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "valp_debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t old_size, size_t new_size) {
  vm.bytes_allocated += new_size - old_size;

  if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
  collect_garbage();
#endif

    if (vm.bytes_allocated > vm.next_gc) {
      collect_garbage();
    }
  }

  if (new_size == 0) {
    free(pointer);
    return NULL;
  }

  void *result = realloc(pointer, new_size);
  if (result == NULL) exit(1);
  return result;
}

static void free_object(valp_obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      FREE(valp_bound_method, object);
      break;
    }
    case OBJ_CLASS: {
      valp_class *klass = (valp_class*)object;
      free_hash(&klass->methods);
      FREE(valp_class, object);
      break;
    }
    case OBJ_CLOSURE: {
      valp_obj_closure *closure = (valp_obj_closure*)object;
      FREE_ARRAY(valp_obj_upvalue*, closure->upvalues, closure->upvalue_count);
      FREE(valp_obj_closure, object);
      break;
    }
    case OBJ_FUNCTION: {
      valp_function *function = (valp_function*)object;
      free_bytecode(&function->bytecode);
      FREE(valp_function, object);
      break;
    }
    case OBJ_INSTANCE: {
      valp_instance *instance = (valp_instance*)object;
      free_hash(&instance->fields);
      FREE(valp_instance, object);
      break;
    }
    case OBJ_NATIVE: {
      FREE(valp_obj_native, object);
      break;
    }
    case OBJ_STRING: {
      valp_string *string = (valp_string*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(valp_string, object);
      break;
    }
    case OBJ_UPVALUE: {
      FREE(valp_obj_upvalue, object);
      break;
    }
  }
}

void mark_object(valp_obj *object) {
  if (object == NULL) return;
  if (object->is_marked) return;

#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  print_value(OBJ_VAL(object));
  printf("\n");
#endif

  object->is_marked = true;

  if (vm.gray_capacity < vm.gray_count + 1) {
    vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
    vm.gray_stack = realloc(vm.gray_stack, sizeof(valp_obj*) * vm.gray_capacity);

    if (vm.gray_stack == NULL) exit(1);
  }

  vm.gray_stack[vm.gray_count++] = object;
}

void mark_value(valp_value value) {
  if (!IS_OBJ(value)) return;
  mark_object(AS_OBJ(value));
}

static void mark_array(valp_value_array *array) {
  for (int i = 0; i < array->count; i++) {
    mark_value(array->values[i]);
  }
}

static void blacken_object(valp_obj *object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  print_value(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      valp_bound_method *bound = (valp_bound_method*)object;
      mark_value(bound->receiver);
      mark_object((valp_obj*)bound->method);
      break;
    }
    case OBJ_CLASS: {
      valp_class *klass = (valp_class*)object;
      mark_object((valp_obj*)klass->name);
      mark_hash(&klass->methods);
      break;
    }
    case OBJ_CLOSURE: {
      valp_obj_closure *closure = (valp_obj_closure*)object;
      mark_object((valp_obj*)closure->function);
      for (int i = 0; i < closure->upvalue_count; i++) {
        mark_object((valp_obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      valp_function *function = (valp_function*)object;
      mark_object((valp_obj*)function->name);
      mark_array(&function->bytecode.constants);
      break;
    }
    case OBJ_INSTANCE: {
      valp_instance *instance = (valp_instance*)object;
      mark_object((valp_obj*)instance->klass);
      mark_hash(&instance->fields);
      break;
    }
    case OBJ_UPVALUE: {
      mark_value(((valp_obj_upvalue*)object)->closed);
      break;
    }
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
  }
}

static void mark_roots() {
  for (valp_value *slot = vm.stack; slot < vm.stack_top; slot++) {
    mark_value(*slot);
  }

  for (int i = 0; i < vm.frame_count; i++) {
    mark_object((valp_obj*)vm.frames[i].closure);
  }

  for (valp_obj_upvalue *upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
    mark_object((valp_obj*)upvalue);
  }

  mark_hash(&vm.globals);
  mark_compiler_roots();
  mark_object((valp_obj*)vm.init_string);
}

static void trace_references() {
  while (vm.gray_count > 0) {
    valp_obj *object = vm.gray_stack[--vm.gray_count];
    blacken_object(object);
  }
}

static void sweep() {
  valp_obj *previous = NULL;
  valp_obj *object = vm.objects;
  
  while (object != NULL) {
    if (object->is_marked) {
      object->is_marked = false;
      previous = object;
      object = object->next;
    } else {
      valp_obj *unreached = object;

      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }

      free_object(unreached);
    }
  }
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
  printf("== GC BEGIN ==\n");
  size_t before = vm.bytes_allocated;
#endif

  mark_roots();
  trace_references();
  hash_remove_white(&vm.strings);
  sweep();

  vm.next_gc = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("==  GC END  ==\n");
  printf("   collected %ld bytes (from %ld to %ld) next at %ld\n",
           before - vm.bytes_allocated, before, vm.bytes_allocated,
           vm.next_gc);
#endif
}

void free_objects() {
  valp_obj *object = vm.objects;
  while (object != NULL) {
    valp_obj *next = object->next;
    free_object(object);
    object = next;
  }

  free(vm.gray_stack);
}
