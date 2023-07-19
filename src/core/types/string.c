#include <stdio.h>
#include <string.h>

#include "string.h"
#include "../valp_native.h"
#include "../valp_vm.h"

static valp_value string_length(int arg_count, valp_value *args) {
  if (arg_count != 0) {
    runtime_error("size() takes 0 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_string *str = AS_STRING(args[0]);
  
  return NUMBER_VAL(str->length);
}

static valp_value string_reverse(int arg_count, valp_value *args) {
  if (arg_count != 0) {
    runtime_error("reverse() takes 0 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_string *str = AS_STRING(args[0]);
  
  int length = strlen(str->chars);
  char temp;

  for (int i = 0, j = length - 1; i < j; i++, j--) {
      temp = str->chars[i];
      str->chars[i] = str->chars[j];
      str->chars[j] = temp;
  }
  
  return OBJ_VAL(str);
}

static valp_value string_replace(int arg_count, valp_value *args) {
  if (arg_count != 2) {
    runtime_error("replace() takse 2 arguments, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  valp_string *str = AS_STRING(args[0]);

  if (!IS_STRING(args[1]) || !IS_STRING(args[2])) {
    runtime_error("replace() takes two strings as an arguments.");
    return UNDEFINED_VAL;
  }

  valp_string *arg1 = AS_STRING(args[1]);
  valp_string *arg2 = AS_STRING(args[2]);

  if (arg1->length != 1 || arg2->length != 1) {
    runtime_error("replace() takes single characters as an arguments.");
    return UNDEFINED_VAL;
  }

  for (int i = 0; i < str->length; ++i) {
    if (str->chars[i] == arg1->chars[0]) {
      str->chars[i] = arg2->chars[0];
    }
  }

  return OBJ_VAL(str);
}

static void get_slice(const char *source, int start, int end, char *destination) {
  int j = 0;
  for (int i = start; i <= end && source[i] != '\0'; i++) {
    destination[j] = source[i];
    j++;
  }
  destination[j] = '\0';
}

static valp_value string_split(int arg_count, valp_value *args) {
  if (arg_count != 1) {
    runtime_error("split() takse 1 argument, given %d", arg_count);
    return UNDEFINED_VAL;
  }

  if (!IS_STRING(args[1])|| AS_STRING(args[1])->length != 1) {
    runtime_error("split() argument must be single character");
    return UNDEFINED_VAL;
  }

  valp_array *arr = new_array();
  valp_string *str = AS_STRING(args[0]);
  valp_string *arg = AS_STRING(args[1]);
  int prev_begin = 0;

  for (int i = 0; i <= str->length; ++i) {
    if (str->chars[i] == arg->chars[0] || i == str->length) {
      
      char slice[i - prev_begin + 1];
      get_slice(str->chars, prev_begin, i - 1, slice);
      
      valp_string *new_str = copy_string(slice, i);
      write_valp_value_array(&arr->values, OBJ_VAL(new_str));

      prev_begin = i + 1;
      ++i;
    }
  }

  return OBJ_VAL(arr);
}

void define_string_methods() {
  define_native(&vm.string_methods, "len", string_length);
  define_native(&vm.string_methods, "reverse", string_reverse);
  define_native(&vm.string_methods, "replace", string_replace);
  define_native(&vm.string_methods, "split", string_split);
}