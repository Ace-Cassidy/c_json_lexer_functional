#include "lex.h"

/*TOKEN*/

//// tkn_create
// we are passing the address of the token contained in the lexer
// this works because we only send one token at a time
// it would be better to buffer the channel and make this memory threadsafe
Token *tkn_create(Lexer *lx, int type) {
  lx->tkn = (Token){type, lx->token_start, lx->current_pos};
  return &lx->tkn;
}

/*LEXER*/

//// lex
// the main entry point
Lexer *lex(FILE *f) {
  Lexer *lx = lx_create(f);
  pthread_t th;
  pthread_create(&th, NULL, (void *)lx_run, lx);
  return lx;
}

//// lx_create
// initialize the lexer
Lexer *lx_create(FILE *f) {
  Lexer *lx = (Lexer *)malloc(sizeof(Lexer));
  // allocate channel
  lx->emitter = chan_init(0);
  // read input string
  fseek(f, 0, SEEK_END);
  lx->text_length = ftell(f);
  fseek(f, 0, SEEK_SET);
  lx->input_text = (char *)malloc(lx->text_length);
  fread(lx->input_text, 1, lx->text_length, f);
  fclose(f);
  // other fields
  lx->current_pos = 0;
  lx->token_start = 0;

  return lx;
}

//// lx_run
// loop through state functions until null is returned
void *lx_run(Lexer *lx) {
  for (StateFn sf = (StateFn){state_start}; sf.ptr;) {
    sf = sf.ptr(lx);
  }
  chan_close(lx->emitter);
}

//// lx_next
// move forward one character
// lexer does not support wide characters (unicode)
char lx_next(Lexer *lx) {
  lx->current_pos++;
  if (lx->current_pos >= lx->text_length - 1) {
    lx->atEOF = true;
  } else {
    char c = lx->input_text[lx->current_pos];
    return c;
  }
}

//// lx_current
// return character at current position
char lx_current(Lexer *lx) {
  char c = lx->input_text[lx->current_pos];
  return c;
}

//// lx_backup
// move backwards one character
void *lx_backup(Lexer *lx) { lx->current_pos--; }

//// lx_ignore
// set token start to current position, ignore token up to this point
void *lx_ignore(Lexer *lx) { lx->token_start = lx->current_pos; }

//// lx_emit
// emit a token and return the state passed in
StateFn lx_emit(Lexer *lx, TokenType id, StateFn next_state) {
  Token *tkn = tkn_create(lx, id);
  chan_send(lx->emitter, (void *)tkn);
  lx->token_start = ++lx->current_pos;
  if (lx->atEOF)
    return (StateFn){NULL};
  return next_state;
}

/* StateFn */

//// whitespace_map
static const StateFn whitespace_map[256] = {
    [0 ... 255] = (StateFn){state_start}, [' '] = (StateFn){state_whitespace},
    ['\t'] = (StateFn){state_whitespace}, ['\r'] = (StateFn){state_whitespace},
    ['\n'] = (StateFn){state_whitespace},
};

//// state_whitespace
// consume whitespace but don't emit a token
StateFn state_whitespace(Lexer *lx) {
  char c = lx_next(lx);
  lx_ignore(lx);
  return whitespace_map[c];
};

/* Terminal */

//// state_colon
StateFn state_colon(Lexer *lx) {
  return lx_emit(lx, TypeColon, (StateFn){state_start});
}

//// state_comma
StateFn state_comma(Lexer *lx) {
  return lx_emit(lx, TypeComma, (StateFn){state_start});
};

//// state_lcurly
StateFn state_lcurly(Lexer *lx) {
  return lx_emit(lx, TypeLCurly, (StateFn){state_start});
}

//// state_rcurly
StateFn state_rcurly(Lexer *lx) {
  return lx_emit(lx, TypeRCurly, (StateFn){state_start});
}

//// state_lsquare
StateFn state_lsquare(Lexer *lx) {
  return lx_emit(lx, TypeLSquare, (StateFn){state_start});
}

//// state_rsquare
StateFn state_rsquare(Lexer *lx) {
  return lx_emit(lx, TypeRSquare, (StateFn){state_start});
}

/* Number */

//// state_zero
// zero must be special to disallow integers starting with zero
StateFn state_zero(Lexer *lx) {
  char c = lx_next(lx);
  if (isdigit(c)) // don't allow integers that start with 0
    return (StateFn){state_error};
  if (c == '.') //  allow floats that start with 0
    return (StateFn){state_float};
  else {
    lx_emit(lx, TypeInteger, (StateFn){state_reset});
  }
};

//// state_sign
// for negative integers
StateFn state_sign(Lexer *lx) {
  char c = lx_next(lx);
  if (isdigit(c)) {
    return (StateFn){state_integer};
  } else {
    return (StateFn){state_error};
  }
}

//// state_integer
// all numbers are integers until proven otherwise
StateFn state_integer(Lexer *lx) {
  char c = lx_next(lx);
  while (isdigit(c)) {
    c = lx_next(lx);
  }
  if (c == '.') {
    return (StateFn){state_float};
  } else if (c == 'e' || c == 'E') {
    sub_exponent(lx, (StateFn){state_integer});
  } else {
    lx_backup(lx);
    return lx_emit(lx, TypeInteger, (StateFn){state_reset});
  }
}

//// state_float
StateFn state_float(Lexer *lx) {
  char c = lx_next(lx);
  while (isdigit(c))
    c = lx_next(lx);
  if (c == 'e' || c == 'E') {
    sub_exponent(lx, (StateFn){state_float});
  }
  lx_backup(lx);
  return lx_emit(lx, TypeFloat, (StateFn){state_reset});
}

//// consume_exponent
// a sub state function
StateFn sub_exponent(Lexer *lx, StateFn prev_state) {
  char c = lx_next(lx);
  if (c == '-' || c == '+')
    c = lx_next(lx);
  if (!isdigit(c)) // need at least one digit after exponent or else error
    return (StateFn){state_error};
  while (isdigit(c))
    c = lx_next(lx);
  return prev_state;
}

/* String */

//// state_dq_string
// double quoted string
StateFn state_dq_string(Lexer *lx) {
  char c;
  for (;;) {
    c = lx_next(lx);
    if (c == '\\')
      c = lx_next(lx);
    else if (c == '"') {
      return lx_emit(lx, TypeString, (StateFn){state_reset});
    }
  }
}

//// state_sq_string
// single quoted string
StateFn state_sq_string(Lexer *lx) {
  char c;
  for (;;) {
    c = lx_next(lx);
    if (c == '\\')
      c = lx_next(lx);
    else if (c == '\'') {
      return lx_emit(lx, TypeString, (StateFn){state_reset});
    }
  }
}

//// state_error
// when we see something we can't lex return error token and exit
StateFn state_error(Lexer *lx) {
  return lx_emit(lx, TypeError, (StateFn){NULL});
};

//// state_keyword
// treat any unquoted string as a keyword
// should be only null, true, false
StateFn state_keyword(Lexer *lx) {
  char c;
  while (isalpha((c = lx_next(lx))))
    ;
  lx_backup(lx);
  return lx_emit(lx, TypeKeyword, (StateFn){state_reset});
}

static const StateFn reset_map[256] = {
    [0 ... 255] = (StateFn){state_error}, ['}'] = (StateFn){state_start},
    [']'] = (StateFn){state_start},       [','] = (StateFn){state_start},
    [':'] = (StateFn){state_start},       [' '] = (StateFn){state_start},
    ['\t'] = (StateFn){state_start},      ['\r'] = (StateFn){state_start},
    ['\n'] = (StateFn){state_start},
};

//// state_reset
// look to see if we ended last token correctly
StateFn state_reset(Lexer *lx) {
  char c = lx_current(lx);
  return reset_map[c];
}

//// start_map
static const StateFn start_map[256] = {
    [0 ... 255] = (StateFn){state_error},
    ['"'] = (StateFn){state_dq_string},
    ['\''] = (StateFn){state_sq_string},
    ['0'] = (StateFn){state_zero},
    ['1' ... '9'] = (StateFn){state_integer},
    ['-'] = (StateFn){state_sign},
    ['{'] = (StateFn){state_lcurly},
    ['}'] = (StateFn){state_rcurly},
    ['['] = (StateFn){state_lsquare},
    [']'] = (StateFn){state_rsquare},
    [','] = (StateFn){state_comma},
    [':'] = (StateFn){state_colon},
    ['a' ... 'z'] = (StateFn){state_keyword},
    [' '] = (StateFn){state_whitespace},
    ['\t'] = (StateFn){state_whitespace},
    ['\r'] = (StateFn){state_whitespace},
    ['\n'] = (StateFn){state_whitespace},
};

//// state_start
// the state we start in and return to
StateFn state_start(Lexer *lx) {
  char c = lx_current(lx);
  return start_map[c];
};
