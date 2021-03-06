#include "../chan/chan.h"
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef chan_t Channel;

typedef enum {
  TypeError = 'S',
  TypeInteger = 'I',
  TypeFloat = 'F',
  TypeKeyword = 'K',
  TypeString = 'S',
  TypeLCurly = '{',
  TypeRCurly = '}',
  TypeLSquare = '[',
  TypeRSquare = ']',
  TypeColon = ':',
  TypeComma = ','
} TokenType;

typedef struct {
  TokenType type;
  unsigned int start, end;
} Token;

typedef struct {
  Channel *emitter;
  char *input_text;
  unsigned int text_length;
  unsigned int current_pos;
  unsigned int token_start;
  bool atEOF;
  Token tkn;
} Lexer;

struct _StateFn {
  struct _StateFn (*ptr)(Lexer *lx);
};
typedef struct _StateFn StateFn;

char lx_current(Lexer *lx);
char lx_next(Lexer *lx);
Lexer *lex(FILE *f);
Lexer *lx_create(FILE *f);
StateFn lx_emit(Lexer *lx, TokenType id, StateFn next_state);
StateFn state_colon(Lexer *lx);
StateFn state_comma(Lexer *lx);
StateFn state_dq_string(Lexer *lx);
StateFn state_error(Lexer *lx);
StateFn state_float(Lexer *lx);
StateFn state_integer(Lexer *lx);
StateFn state_keyword(Lexer *lx);
StateFn state_lcurly(Lexer *lx);
StateFn state_lsquare(Lexer *lx);
StateFn state_rcurly(Lexer *lx);
StateFn state_reset(Lexer *lx);
StateFn state_rsquare(Lexer *lx);
StateFn state_sign(Lexer *lx);
StateFn state_sq_string(Lexer *lx);
StateFn state_start(Lexer *lx);
StateFn state_whitespace(Lexer *lx);
StateFn state_zero(Lexer *lx);
StateFn sub_exponent(Lexer *lx, StateFn prev_state);
Token *tkn_create(Lexer *lx, int type);
void *lx_backup(Lexer *lx);
void *lx_ignore(Lexer *lx);
void *lx_run(Lexer *lx);