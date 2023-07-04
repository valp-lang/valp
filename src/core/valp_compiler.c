#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/valp.h"
#include "valp_compiler.h"
#include "valp_memory.h"
#include "valp_scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "valp_debug.h"
#endif

typedef struct {
  valp_token current;
  valp_token previous;
  bool had_error;
  bool panic_mode;
} valp_parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISION, // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} valp_precedence;

typedef void (*valp_parse_fn)(bool can_assign);

typedef struct {
  valp_parse_fn prefix;
  valp_parse_fn infix;
  valp_precedence precedence;
} valp_parse_rule;

typedef struct {
  valp_token name;
  int depth;
  bool is_captured;
  bool constant;
} valp_local;

typedef struct {
  uint8_t index;
  bool is_local;
  bool constant;
} valp_upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT
} valp_function_type;

typedef struct valp_loop_s {
  int start;
  int exit;
  int scope_depth;
  struct valp_loop_s *enclosing;
} valp_loop;

typedef struct valp_compiler {
  struct valp_compiler *enclosing;
  valp_function *function;
  valp_function_type type;

  // Loop being currently executed, NULL if there are none
  valp_loop* loop;

  valp_local locals[UINT8_COUNT];
  int local_count;
  valp_upvalue upvalues[UINT8_COUNT];
  int scope_depth;
} valp_compiler;

typedef struct valp_class_compiler {
  struct valp_class_compiler *enclosing;
  valp_token name;
  bool has_superclass;
} valp_class_compiler;

valp_parser parser;

valp_compiler *current = NULL;

valp_class_compiler *current_class = NULL;

static valp_bytecode *current_bytecode() {
  return &current->function->bytecode;
}

static void error_at(valp_token *token, const char *message) {
  if (parser.panic_mode) return;
  parser.panic_mode = true;

  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.had_error = true;
}

static void error(const char *message) {
  error_at(&parser.previous, message);
}

static void error_at_current(const char *message) {
  error_at(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for(;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR) break;

    error_at_current(parser.current.start);
  }
}

static void consume(valp_token_type type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  error_at_current(message);
}

static bool check(valp_token_type type) {
  return parser.current.type == type;
}

static bool match(valp_token_type type) {
  if (!check(type)) return false;
  advance();
  
  return true;
}

static void emit_byte(uint8_t byte) {
  write_bytecode(current_bytecode(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_loop(int loop_start) {
  emit_byte(OP_LOOP);

  int offset = current_bytecode()->count - loop_start + 2;
  if (offset > UINT16_MAX) error("Loop body too large");

  emit_byte((offset >> 8) & 0xff);
  emit_byte(offset & 0xff);
}

static int emit_jump(uint8_t instruction) {
  emit_byte(instruction);
  emit_byte(0xff);
  emit_byte(0xff);
  return current_bytecode()->count - 2;
}

static void emit_return() {
  if (current->type == TYPE_INITIALIZER) {
    emit_bytes(OP_GET_LOCAL, 0);
  } else {
    emit_byte(OP_NIL);
  }

  emit_byte(OP_RETURN);
}

static uint8_t make_constant(valp_value value) {
  int constant = add_constant(current_bytecode(), value);
  if (constant > UINT8_MAX) {
    error("Too many constants in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emit_constant(valp_value value) {
  emit_bytes(OP_CONSTANT, make_constant(value));
}

static void patch_jump(int offset) {
  // -2 to adjust for the bytecode for the jump offset itself.
  int jump = current_bytecode()->count - offset - 2;

  if (jump > UINT16_MAX) {
    error("Too much code to jump over");
  }

  current_bytecode()->code[offset] = (jump >> 8) & 0xff;
  current_bytecode()->code[offset + 1] = jump & 0xff;
}

static void init_compiler(valp_compiler *compiler, valp_function_type type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->local_count = 0;
  compiler->scope_depth = 0;
  compiler->function = new_function();
  compiler->loop = NULL;

  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name = copy_string(parser.previous.start, parser.previous.length);
  }

  valp_local *local = &current->locals[current->local_count++];
  local->depth = 0;
  local->is_captured = false;

  if (type != TYPE_FUNCTION) {
    local->name.start = "self";
    local->name.length = 4;
  } else {
    local->name.start = "";
    local->name.length = 0;
  }
}

static valp_function *end_compiler() {
  emit_return();
  valp_function *function = current->function;
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_bytecode(current_bytecode(), function->name != NULL ? function->name->chars : "<script>");
  }
#endif

  current = current->enclosing;
  return function;
}

static void begin_scope() {
  current->scope_depth++;
}

static void end_scope() {
  current->scope_depth--;

  while (current->local_count > 0
          && current->locals[current->local_count - 1].depth > current->scope_depth) {
    if (current->locals[current->local_count - 1].is_captured) {
      emit_byte(OP_CLOSE_UPVALUE);
    } else {
      emit_byte(OP_POP);
    }

    current->local_count--;
  }
}

static void expression();
static void statement();
static void declaration();
static uint8_t identifier_constant(valp_token *name);
static valp_parse_rule *get_rule(valp_token_type type);
static void parse_precedence(valp_precedence precedence);

static void binary(bool can_assign) {
  // Remember the operator.
  valp_token_type operator_type = parser.previous.type;

  // Compile the right operand.
  valp_parse_rule *rule = get_rule(operator_type);
  parse_precedence((valp_precedence)(rule->precedence + 1));

  // Emit the operator instruction.
  switch (operator_type) {
    case TOKEN_BANG_EQUAL:    emit_bytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emit_byte(OP_EQUAL); break;
    case TOKEN_GREATER:       emit_byte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emit_byte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emit_bytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:          emit_byte(OP_ADD); break;
    case TOKEN_MINUS:         emit_byte(OP_SUBTRACT); break;
    case TOKEN_STAR:          emit_byte(OP_MULTIPLY); break;
    case TOKEN_SLASH:         emit_byte(OP_DIVIDE); break;
    default:
      return;
  }
}

static uint8_t argument_list() {
  uint8_t arg_count = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();

      if (arg_count == 255) error("Can't have more than 255 arguments.");

      arg_count++;
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return arg_count;
}

static void call(bool can_assign) {
  uint8_t arg_count = argument_list();
  emit_bytes(OP_CALL, arg_count);
}

static void dot(bool can_assign) {
  consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
  uint8_t name = identifier_constant(&parser.previous);

  if (can_assign && match(TOKEN_EQUAL)) {
    expression();
    emit_bytes(OP_SET_PROPERTY, name);
  } else if (can_assign && match(TOKEN_PLUS_EQUAL)) {
    emit_bytes(OP_GET_PROPERTY_NO_POP, name);
    expression();
    emit_byte(OP_ADD);
    emit_bytes(OP_SET_PROPERTY, name);
  } else if (can_assign && match(TOKEN_MINUS_EQUAL)) {
    emit_bytes(OP_GET_PROPERTY_NO_POP, name);
    expression();
    emit_byte(OP_SUBTRACT);
    emit_bytes(OP_SET_PROPERTY, name);
  } else if (can_assign && match(TOKEN_SLASH_EQUAL)) {
    emit_bytes(OP_GET_PROPERTY_NO_POP, name);
    expression();
    emit_byte(OP_SUBTRACT);
    emit_bytes(OP_SET_PROPERTY, name);
  } else if (can_assign && match(TOKEN_STAR_EQUAL)) {
    emit_bytes(OP_GET_PROPERTY_NO_POP, name);
    expression();
    emit_byte(OP_MULTIPLY);
    emit_bytes(OP_SET_PROPERTY, name);
  } else if (match(TOKEN_LEFT_PAREN)) {
    uint8_t arg_count = argument_list();
    emit_bytes(OP_INVOKE, name);
    emit_byte(arg_count);
  } else {
    emit_bytes(OP_GET_PROPERTY, name);
  }
}

static void literal(bool can_assign) {
  switch (parser.previous.type) {
    case TOKEN_FALSE: emit_byte(OP_FALSE); break;
    case TOKEN_NIL:   emit_byte(OP_NIL); break;
    case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
    default: return;
  }
}

static void grouping(bool can_assign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign) {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

static void and_(bool can_assign) {
  int end_jump = emit_jump(OP_JUMP_IF_FALSE);

  emit_byte(OP_POP);
  parse_precedence(PREC_AND);

  patch_jump(end_jump);
}

static void or_(bool can_assign) {
  int else_jump = emit_jump(OP_JUMP_IF_FALSE);
  int end_jump = emit_jump(OP_JUMP);

  patch_jump(else_jump);
  emit_byte(OP_POP);

  parse_precedence(PREC_OR);
  patch_jump(end_jump);
}

static void string(bool can_assign) {
  emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static bool identifiers_equal(valp_token *a, valp_token *b) {
  if (a->length != b->length) return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(valp_compiler *compiler, valp_token *name) {
  for (int i = compiler->local_count - 1; i >= 0; i--) {
    valp_local *local = &compiler->locals[i];
    if (identifiers_equal(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }

  return -1;
}

static int add_upvalue(valp_compiler *compiler, uint8_t index, bool is_local, bool constant) {
  int upvalue_count = compiler->function->upvalue_count;

  for (int i = 0; i < upvalue_count; i++) {
    valp_upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) {
      return i;
    }
  }

  if (upvalue_count == UINT8_COUNT) {
    error("Too many closure variables in function.");
    return 0;
  }

  compiler->upvalues[upvalue_count].is_local = is_local;
  compiler->upvalues[upvalue_count].index = index;
  compiler->upvalues[upvalue_count].constant = constant;

  return compiler->function->upvalue_count++;
}

static void check_constant(uint8_t set_op, int arg) {
  if (set_op == OP_SET_LOCAL) {
    if (current->locals[arg].constant) {
      error("Cannot assign to a constant.");
    }
  } else if (set_op == OP_SET_UPVALUE) {
    valp_upvalue upvalue = current->upvalues[arg];

    if (upvalue.constant) {
      error("Cannot assign to a constant.");
    }
  } else if (set_op == OP_SET_GLOBAL) {
    valp_value _;
    if (hash_get(&vm.constants, AS_STRING(current_bytecode()->constants.values[arg]), &_)) {
      error("Cannot assign to a constant.");
    }
  }
}

static int resolve_upvalue(valp_compiler *compiler, valp_token *name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolve_local(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].is_captured = true;
    return add_upvalue(compiler, (uint8_t)local, true, compiler->enclosing->locals[local].constant);
  }

  int upvalue = resolve_upvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return add_upvalue(compiler, (uint8_t)upvalue, false, compiler->enclosing->upvalues[upvalue].constant);
  }

  return -1;
}

static void named_variable(valp_token name, bool can_assign) {
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);
  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  } else if ((arg = resolve_upvalue(current, &name)) != -1) {
    get_op = OP_GET_UPVALUE;
    set_op = OP_SET_UPVALUE;
  } else {
    arg = identifier_constant(&name);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  }
  
  if (can_assign && match(TOKEN_EQUAL)) {
    check_constant(set_op, arg);
    expression();
    emit_bytes(set_op, (uint8_t)arg);
  } else if (can_assign && match(TOKEN_PLUS_EQUAL)) {
    check_constant(set_op, arg);
    named_variable(name, false);
    expression();
    emit_byte(OP_ADD);
    emit_bytes(set_op, (uint8_t)arg);
  } else if (can_assign && match(TOKEN_MINUS_EQUAL)) {
    check_constant(set_op, arg);
    named_variable(name, false);
    expression();
    emit_byte(OP_SUBTRACT);
    emit_bytes(set_op, (uint8_t)arg);
  } else if (can_assign && match(TOKEN_SLASH_EQUAL)) {
    check_constant(set_op, arg);
    named_variable(name, false);
    expression();
    emit_byte(OP_DIVIDE);
    emit_bytes(set_op, (uint8_t)arg);
  } else if (can_assign && match(TOKEN_STAR_EQUAL)) {
    check_constant(set_op, arg);
    named_variable(name, false);
    expression();
    emit_byte(OP_DIVIDE);
    emit_bytes(set_op, (uint8_t)arg);
  } else {
    emit_bytes(get_op, (uint8_t)arg);
  }
}

static void variable(bool can_assign) {
  named_variable(parser.previous, can_assign);
}

static valp_token synthetic_token(const char* text) {
  valp_token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void super_(bool can_assign) {
  if (current_class == NULL) {
    error("Can't use 'super' outside of a class.");
  } else if (!current_class->has_superclass) {
    error("Can'y use 'super' in a class with no superclass.");
  }

  consume(TOKEN_DOT, "Expect '.' after 'super'.");
  consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
  uint8_t name = identifier_constant(&parser.previous);

  named_variable(synthetic_token("self"), false);
  if (match(TOKEN_LEFT_PAREN)) {
    uint8_t arg_count = argument_list();
    named_variable(synthetic_token("super"), false);
    emit_bytes(OP_SUPER_INVOKE, name);
    emit_byte(arg_count);
  } else {
    named_variable(synthetic_token("super"), false);
    emit_bytes(OP_GET_SUPER, name);
  }
}

static void self_(bool can_assign) {
  if (current_class == NULL) {
    error("Can't user 'self' outside of a class.");
    return;
  }

  variable(false);
}

static void unary(bool can_assign) {
  valp_token_type operator_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (operator_type) {
    case TOKEN_BANG:  emit_byte(OP_NOT); break;
    case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
    default:
      return;
  }
}

valp_parse_rule rules[] = {
    [TOKEN_LEFT_PAREN] =    {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN] =   {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE] =    {NULL,     NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE] =   {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT] =           {NULL,     dot,    PREC_CALL},
    [TOKEN_MINUS] =         {unary,    binary, PREC_TERM},
    [TOKEN_PLUS] =          {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON] =     {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH] =         {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR] =          {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG] =          {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL] =    {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL] =   {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER] =       {NULL,     binary, PREC_COMPARISION},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISION},
    [TOKEN_LESS] =          {NULL,     binary, PREC_COMPARISION},
    [TOKEN_LESS_EQUAL] =    {NULL,     binary, PREC_COMPARISION},
    [TOKEN_IDENTIFIER] =    {variable, NULL,   PREC_NONE},
    [TOKEN_STRING] =        {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER] =        {number,   NULL,   PREC_NONE},
    [TOKEN_AND] =           {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE] =          {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE] =         {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR] =           {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN] =           {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF] =            {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL] =           {literal,  NULL,   PREC_NONE},
    [TOKEN_OR] =            {NULL,     or_,    PREC_OR},
    [TOKEN_PRINT] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN] =        {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER] =         {super_,   NULL,   PREC_NONE},
    [TOKEN_SELF] =          {self_,    NULL,   PREC_NONE},
    [TOKEN_TRUE] =          {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR] =           {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF] =           {NULL,     NULL,   PREC_NONE},
    [TOKEN_CONST] =         {NULL,     NULL,   PREC_NONE},
    [TOKEN_PLUS_EQUAL] =    {NULL,     NULL,   PREC_NONE},
    [TOKEN_MINUS_EQUAL] =   {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH_EQUAL] =   {NULL,     NULL,   PREC_NONE},
    [TOKEN_STAR_EQUAL] =    {NULL,     NULL,   PREC_NONE}
};

static void parse_precedence(valp_precedence precedence) {
  advance();
  valp_parse_fn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    error("Expect expression.");
    return;
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(can_assign);

  while (precedence <= get_rule(parser.current.type)->precedence) {
    advance();
    valp_parse_fn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static uint8_t identifier_constant(valp_token *name) {
  return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static void add_local(valp_token name) {
  if (current->local_count == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }

  valp_local *local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
  local->constant = false;
  local->is_captured = false;
}

static void declare_variable() {
  // Global variables are implicitly declared.
  if (current->scope_depth == 0) return;

  valp_token *name = &parser.previous;

  for (int i = current->local_count - 1; i >= 0; i--) {
    valp_local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scope_depth) {
      break;
    }

    if (identifiers_equal(name, &local->name)) { error("Already variable with this name in this scope."); }
  }

  add_local(*name);
}

static uint8_t parse_variable(const char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);

  declare_variable();
  if (current->scope_depth > 0) return 0;

  return identifier_constant(&parser.previous);
}

static void mark_initialized(bool constant) {
  current->locals[current->local_count - 1].depth = current->scope_depth;
  current->locals[current->local_count - 1].constant = constant;
}

static void define_variable(uint8_t global, bool constant) {
  if (current->scope_depth == 0) {
    if (constant) {
      hash_set(&vm.constants, AS_STRING(current_bytecode()->constants.values[global]), NIL_VAL);
    }
  } else {
    mark_initialized(constant);
    return;
  }

  emit_bytes(OP_DEFINE_GLOBAL, global);
}

static valp_parse_rule *get_rule(valp_token_type type) {
  return &rules[type];
}

static void expression() {
  parse_precedence(PREC_ASSIGNMENT);
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void start_loop(valp_loop *loop) {
  loop->start = current_bytecode()->count;
  loop->enclosing = current->loop;
  current->loop = loop;
}

static void end_loop(valp_loop *loop) {
  current->loop = current->loop->enclosing;
}

static void discard_locals() {
  int local = current->local_count - 1;
  while (local >= 0 && current->locals[local].depth >= current->loop->scope_depth) {
    if (current->locals[local].is_captured) {
      emit_byte(OP_CLOSE_UPVALUE);
    } else {
      emit_byte(OP_POP);
    }

    local--;
  }
}

static void next_statement() {
  if (current->loop == NULL) {
    error("Can't 'next' outside of a loop.");
    return;
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after next.");
  // Discard locals
  discard_locals();

  emit_loop(current->loop->start);
}

static void function(valp_function_type type) {
  valp_compiler compiler;
  init_compiler(&compiler, type);
  begin_scope();

  // Compile the parametr list.
  consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > 255) {
        error_at_current("Can't have more than 255 parametets.");
      }

      uint8_t param_constant = parse_variable("Expect parametr name.");
      define_variable(param_constant, false);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

  // The body
  consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
  block();

  // Create the function object;
  valp_function *function = end_compiler();
  emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(function)));

  for (int i = 0; i < function->upvalue_count; i++) {
    emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
    emit_byte(compiler.upvalues[i].index);
  }
}

static void method() {
  consume(TOKEN_DEF, "Expect 'def' before method name.");
  consume(TOKEN_IDENTIFIER, "Expect method name.");
  uint8_t constant = identifier_constant(&parser.previous);

  valp_function_type type = TYPE_METHOD;
  if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
    type = TYPE_INITIALIZER;
  }

  function(type);

  emit_bytes(OP_METHOD, constant);
}

static void class_declaration() {
  consume(TOKEN_IDENTIFIER, "Expect class name.");
  valp_token class_name = parser.previous;

  uint8_t name_constant = identifier_constant(&parser.previous);
  declare_variable();

  emit_bytes(OP_CLASS, name_constant);
  define_variable(name_constant, false);

  valp_class_compiler class_compiler;
  class_compiler.has_superclass = false;
  class_compiler.name = parser.previous;
  class_compiler.enclosing = current_class;
  current_class = &class_compiler;

  if (match(TOKEN_LESS)) {
    consume(TOKEN_IDENTIFIER, "Expect superclass name.");
    variable(false);

    if (identifiers_equal(&class_name, &parser.previous)) { error("A class can't inherit from itself."); }

    begin_scope();
    add_local(synthetic_token("super"));
    define_variable(0, false);

    named_variable(class_name, false);
    emit_byte(OP_INHERIT);
    class_compiler.has_superclass = true;
  }

  named_variable(class_name, false);
  consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");

  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    method();
  }

  consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
  emit_byte(OP_POP);

  if (class_compiler.has_superclass) { end_scope(); }

  current_class = current_class->enclosing;
}

static void fun_declaration() {
  uint8_t global = parse_variable("Expect function name.");
  mark_initialized(false);
  function(TYPE_FUNCTION);
  define_variable(global, false);
}

static void var_declaration(bool constant) {
  uint8_t global = parse_variable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emit_byte(OP_NIL);
  }

  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

  define_variable(global, constant);
}

static void expression_statement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");

  // DONT POP IF NEEDED FOR TERNARY OPERATOR
  if (!check(TOKEN_QUESTION_MARK)) { emit_byte(OP_POP); }
}

static void for_statement() {
  begin_scope();
  bool params = false;

  if (check(TOKEN_LEFT_PAREN)) {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    params = true;
  }
  
  if (match(TOKEN_SEMICOLON)) {
    // on initializer.
  } else if (match(TOKEN_VAR)) {
    var_declaration(false);
  } else {
    expression_statement();
  }

  valp_loop loop;
  start_loop(&loop);
  current->loop->exit = -1;

  if (!match(TOKEN_SEMICOLON)) {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

    // Jump out of the loop if the condition is false.
    current->loop->exit = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP); // Condition.
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    int body_jump = emit_jump(OP_JUMP);

    int increment_start = current_bytecode()->count;
    expression();
    emit_byte(OP_POP);

    if (params) { consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses."); }

    emit_loop(loop.start);
    current->loop->start = increment_start;

    patch_jump(body_jump);
  }

  statement();

  emit_loop(loop.start);

  if (current->loop->exit != -1) {
    patch_jump(current->loop->exit);
    emit_byte(OP_POP);
  }

  end_loop(&loop);
  end_scope();
}

static void ternarty_statement() {
  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  statement();

  int else_jump = emit_jump(OP_JUMP);
  patch_jump(then_jump);
  emit_byte(OP_POP);
  
  consume(TOKEN_COLON, "Expected ':' after statement in ternary operator.");
  statement();

  patch_jump(else_jump);
}

static void if_statement() {
  if (check(TOKEN_LEFT_PAREN)) {
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected '(' after confition");
  } else {
    expression();
  }

  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement();

  int else_jump = emit_jump(OP_JUMP);

  patch_jump(then_jump);
  emit_byte(OP_POP);

  if (match(TOKEN_ELSE)) statement();
  patch_jump(else_jump);
}

static void switch_case() {
  emit_byte(OP_DUP);
  expression();
  consume(TOKEN_COLON, "Expect ':' after case expression.");
  
  emit_byte(OP_EQUAL);
  int then_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  
  statement();
  
  int else_jump = emit_jump(OP_JUMP);

  patch_jump(then_jump);
  emit_byte(OP_POP);

  patch_jump(else_jump);
}

static void switch_statement() {
  if (check(TOKEN_LEFT_PAREN)) {
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'switch'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");
  } else {
    expression();
  }

  consume(TOKEN_LEFT_BRACE, "Expected '{'.");

  while(match(TOKEN_CASE)) {
    switch_case();
  }
  
  consume(TOKEN_RIGHT_BRACE, "Expected '}'.");
}

static void print_statement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(OP_PRINT);
}

static void return_statement() {
  if (current->type == TYPE_SCRIPT) error("Can't return from top-level code.");

  if (match(TOKEN_SEMICOLON)) {
    emit_return();
  } else {
    if (current->type == TYPE_INITIALIZER) { error("Can't return a value from an initializer."); }

    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
    emit_byte(OP_RETURN);
  }
}

static void while_statement() {
  valp_loop loop;
  start_loop(&loop);

  if (check(TOKEN_LEFT_PAREN)) {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
  } else {
    expression();
  }

  current->loop->exit = emit_jump(OP_JUMP_IF_FALSE);

  emit_byte(OP_POP);
  statement();

  emit_loop(loop.start);
  patch_jump(current->loop->exit);
  emit_byte(OP_POP);
  end_loop(&loop);
}

static void synchronize() {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;

    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;
      default:
      ;
    }

    advance();
  }
}

static void declaration() {
  if (match(TOKEN_CLASS)) {
    class_declaration();
  } else if (match(TOKEN_FUN)) {
    fun_declaration();
  } else if (match(TOKEN_VAR)) {
    var_declaration(false);
  } else if (match(TOKEN_CONST)) {
    var_declaration(true);
  } else {
    statement();
  }

  if (parser.panic_mode) synchronize();
}

static void statement() {
  if (match(TOKEN_NEXT)) {
    next_statement();
  } else if (match(TOKEN_PRINT)) {
    print_statement();
  } else if (match(TOKEN_FOR)) {
    for_statement();
  } else if (match(TOKEN_IF)) {
    if_statement();
  } else if (match(TOKEN_RETURN)) {
    return_statement();
  } else if (match(TOKEN_WHILE)) {
    while_statement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    begin_scope();
    block();
    end_scope();
  } else if (match(TOKEN_QUESTION_MARK)) {
    ternarty_statement();
  } else if (match(TOKEN_SWITCH)) {
    switch_statement();
  } else {
    expression_statement();
  }
}

valp_function *compile(const char *source) {
  init_scanner(source);
  valp_compiler compiler;
  init_compiler(&compiler, TYPE_SCRIPT);

  parser.had_error  = false;
  parser.panic_mode = false;

  advance();
  
  while (!match(TOKEN_EOF)) {
    declaration();
  }

  valp_function *function = end_compiler();
  return parser.had_error ? NULL : function;
}

void mark_compiler_roots() {
  valp_compiler *compiler = current;
  while (compiler != NULL) {
    mark_object((valp_obj*)compiler->function);
    compiler = compiler->enclosing;
  }
}
