#include "../chan/chan.h"
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef chan_t Channel;

typedef enum {
  TypeError = 'x',
  TypeInteger = '9',
  TypeFloat = '.',
  TypeKeyword = 'k',
  TypeString = 's',
  TypeLCurly = '{',
  TypeRCurly = '}',
  TypeLSquare = '[',
  TypeRSquare = ']',
  TypeColon = ':',
  TypeComma = ','
} TokenType;

struct _Token {
  TokenType type;
  unsigned int start, end;
};
typedef struct _Token Token;

struct _Lexer {
  Channel *emitter;
  char *input_text;
  unsigned int text_length;
  unsigned int current_pos;
  unsigned int token_start;
  bool atEOF;
  Token tkn;
};
typedef struct _Lexer Lexer;

struct _StateFn {
  struct _StateFn (*ptr)(Lexer *lx);
};
typedef struct _StateFn StateFn;

StateFn state_zero(Lexer *lx);
StateFn state_sign(Lexer *lx);
StateFn state_keyword(Lexer *lx);
StateFn state_sq_string(Lexer *lx);
StateFn state_error(Lexer *lx);
StateFn state_dq_string(Lexer *lx);
StateFn state_recovery(Lexer *lx);
StateFn state_float(Lexer *lx);
StateFn state_digits(Lexer *lx);
StateFn state_rsquare(Lexer *lx);
StateFn state_rcurly(Lexer *lx);
StateFn state_lsquare(Lexer *lx);
StateFn state_lcurly(Lexer *lx);
StateFn state_comma(Lexer *lx);
StateFn state_colon(Lexer *lx);
void *lx_emit(Lexer *lx, TokenType id);
void *lx_ignore(Lexer *lx);
void *lx_backup(Lexer *lx);
char lx_current(Lexer *lx);
char lx_next(Lexer *lx);
StateFn state_start(Lexer *lx);
StateFn state_ignore(Lexer *lx);
void *lx_run(Lexer *lx);
Lexer *lx_create(FILE *f);
Lexer *lex(FILE *f);
Token *tkn_create(Lexer *lx, int type);