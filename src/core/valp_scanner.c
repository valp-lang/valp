#include <stdio.h>
#include <string.h>

#include "../include/valp.h"
#include "valp_scanner.h"

typedef struct {
  const char *start;
  const char *current;
  int line;
} valp_scanner;

valp_scanner scanner;

void init_scanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool is_alpha(char c) {
  return (c >= 'a' && c <= 'z') ||
         (c >= 'A' && c <= 'Z') ||
          c == '_';
}

static bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

static bool is_at_end() {
  return *scanner.current == '\0';
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() {
  return *scanner.current;
}

static char peek_next() {
  if (is_at_end()) return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (is_at_end()) return false;
  if (*scanner.current != expected) return false;

  scanner.current++;
  return true;
}

static valp_token make_token(valp_token_type type) {
  valp_token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;

  return token;
}

static valp_token error_token(const char *message) {
  valp_token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;

  return token;
}

static void skip_whitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        scanner.line++;
        advance();
        break;
      case '/':
        if (peek_next() == '/') {
          while (peek() != '\n' && !is_at_end()) advance();
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

static valp_token_type check_keyword(int start, int len, const char *rest, valp_token_type type) {
  if (scanner.current - scanner.start == start + len &&
        memcmp(scanner.start + start, rest, len) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static valp_token_type identifier_type() {
  switch (scanner.start[0]) {
    case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
    case 'b': return check_keyword(1, 4, "reak", TOKEN_BREAK);
    case 'c':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'l': return check_keyword(2, 3, "ass", TOKEN_CLASS);
          case 'a': return check_keyword(2, 2, "se", TOKEN_CASE);
          case 'o': return check_keyword(2, 3, "nst", TOKEN_CONST);
        }
      }
      break;
    case 'd':
      if (scanner.current - scanner.start > 3) {
        return check_keyword(1, 6, "efault", TOKEN_DEFAULT);
      } else {
        return check_keyword(1, 2, "ef", TOKEN_DEF);
      }
    case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
          case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
          case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
        }
      }
      break;
    case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
    case 'n': 
    if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'i': return check_keyword(2, 1, "l", TOKEN_NIL);
          case 'e': return check_keyword(2, 2, "xt", TOKEN_NEXT);
        }
      }
      break;
    case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
    case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
    case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
    case 's': 
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'u': return check_keyword(2, 3, "per", TOKEN_SUPER);
          case 'e': return check_keyword(2, 2, "lf", TOKEN_SELF);
          case 'w': return check_keyword(2, 4, "itch", TOKEN_SWITCH);
        }
      }
      break;
    case 't': return check_keyword(1, 3, "rue", TOKEN_TRUE);
    case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
  }

  return TOKEN_IDENTIFIER;
}

static valp_token identifier() {
  while (is_alpha(peek()) || is_digit(peek())) advance();

  return make_token(identifier_type());
}

static valp_token number() {
  while (is_digit(peek())) advance();

  if (peek() == '.' && is_digit(peek_next())) {
    advance();

    while (is_digit(peek())) advance();
  }

  return make_token(TOKEN_NUMBER);
}

static valp_token string() {
  while (peek() != '"' && !is_at_end()) {
    if (peek() == '\n') scanner.line++;
    advance();
  }

  if (is_at_end()) return error_token("Unterminated string.");

  advance();
  return make_token(TOKEN_STRING);
}

valp_token scan_token() {
  skip_whitespace();
  scanner.start = scanner.current;

  if (is_at_end()) return make_token(TOKEN_EOF);

  char c = advance();

  if (is_alpha(c)) return identifier();
  if (is_digit(c)) return number();

  switch (c) {
    case '(': return make_token(TOKEN_LEFT_PAREN);
    case ')': return make_token(TOKEN_RIGHT_PAREN);
    case '{': return make_token(TOKEN_LEFT_BRACE);
    case '}': return make_token(TOKEN_RIGHT_BRACE);
    case '[': return make_token(TOKEN_LEFT_BRACKET);
    case ']': return make_token(TOKEN_RIGHT_BRACKET);
    case ';': return make_token(TOKEN_SEMICOLON);
    case ',': return make_token(TOKEN_COMMA);
    case '.': return make_token(TOKEN_DOT);
    case '-': return make_token(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
    case '+': return make_token(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
    case '/': return make_token(match('=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH);
    case '*': return make_token(match('=') ? TOKEN_STAR_EQUAL : TOKEN_STAR);
    case '?': return make_token(TOKEN_QUESTION_MARK);
    case ':': return make_token(TOKEN_COLON);
    case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
  }

  return error_token("Unexpected character.");
}
