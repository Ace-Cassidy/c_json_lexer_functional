#include "lex.h"

/*TOKEN*/

//////// tkn_create //
////
Token *tkn_create(Lexer *lx, int type) {
  lx->tkn = (Token){type, lx->token_start, lx->current_pos};
  return &lx->tkn;
}

/*LEXER*/

//////// lex //
////
Lexer *lex(FILE *f) {
  Lexer *lx = lx_create(f);
  pthread_t th;
  pthread_create(&th, NULL, (void *)lx_run, lx);
  return lx;
}

//////// lx_create //
////
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

//////// lx_run //
////
void *lx_run(Lexer *lx) {
  for (StateFn sf = (StateFn){state_start}; sf.ptr;) {
    sf = sf.ptr(lx);
  }
  chan_close(lx->emitter);
}

//////// lx_next //
////
char lx_next(Lexer *lx) {
  if (lx->current_pos >= lx->text_length) {
    lx->atEOF = true;
    lx_emit(lx, TypeError);
    exit(1);
  } else {
    char c = lx->input_text[++lx->current_pos];
    return c;
  }
}

//////// lx_current //
////
char lx_current(Lexer *lx) {
  char c = lx->input_text[lx->current_pos];
  return c;
}

//////// lx_backup //
////
void *lx_backup(Lexer *lx) {
  lx->current_pos--;
  lx->token_start--;
}

//////// lx_ignore //
////
void *lx_ignore(Lexer *lx) { lx->token_start = lx->current_pos; }

//////// lx_emit //
////
void *lx_emit(Lexer *lx, TokenType id) {
  Token *tkn = tkn_create(lx, id);
  chan_send(lx->emitter, (void *)tkn);
  lx->token_start = ++lx->current_pos;
}

/*StateFn*/

/* Terminals */
StateFn state_colon(Lexer *lx) {
  lx_emit(lx, TypeColon);
  return (StateFn){state_start};
}
StateFn state_comma(Lexer *lx) {
  lx_emit(lx, TypeComma);
  return (StateFn){state_start};
};
StateFn state_lcurly(Lexer *lx) {
  lx_emit(lx, TypeLCurly);
  return (StateFn){state_start};
}
StateFn state_lsquare(Lexer *lx) {
  lx_emit(lx, TypeLSquare);
  return (StateFn){state_start};
}
StateFn state_rcurly(Lexer *lx) {
  lx_emit(lx, TypeRCurly);
  return (StateFn){state_start};
}
StateFn state_rsquare(Lexer *lx) {
  lx_emit(lx, TypeRSquare);
  return (StateFn){state_start};
}

/* Number */
StateFn state_digits(Lexer *lx) {
  char c = lx_next(lx);
  if (isdigit(c))
    return (StateFn){state_digits};
  else if (c == '.' || c == 'e' || c == 'E')
    return (StateFn){state_float};
  else {
    lx_emit(lx, TypeInteger);
    return (StateFn){state_recovery};
  }
}

StateFn state_float(Lexer *lx) {
  char c = lx_next(lx);
  if (c == '-' || c == '+')
    c = lx_next(lx);
  while (isdigit(c))
    c = lx_next(lx);
  lx_emit(lx, TypeFloat);
  return (StateFn){state_recovery};
}

/* String */
StateFn state_dq_string(Lexer *lx) {
  char c;
  for (;;) {
    c = lx_next(lx);
    if (c == '\\')
      c = lx_next(lx);
    else if (c == '"') {
      lx_emit(lx, TypeString);
      return (StateFn){state_recovery};
    }
  }
}
StateFn state_sq_string(Lexer *lx) {
  char c;
  for (;;) {
    c = lx_next(lx);
    if (c == '\\')
      c = lx_next(lx);
    else if (c == '\'') {
      lx_emit(lx, TypeString);
      return (StateFn){state_recovery};
    }
  }
}

StateFn state_error(Lexer *lx) {
  lx_emit(lx, TypeError);
  return (StateFn){NULL};
};

StateFn state_keyword(Lexer *lx) {
  char c;
  while (isalpha((c = lx_next(lx))))
    ;
  lx_emit(lx, TypeKeyword);
  return (StateFn){state_recovery};
}

static const StateFn recovery_states[256] = {
    [0 ... 255] = (StateFn){state_error}, ['}'] = (StateFn){state_start},
    [']'] = (StateFn){state_start},       [','] = (StateFn){state_start},
    [':'] = (StateFn){state_start},       [' '] = (StateFn){state_start},
    ['\t'] = (StateFn){state_start},      ['\r'] = (StateFn){state_start},
    ['\n'] = (StateFn){state_start},
};

// look to see if we ended last token correctly
StateFn state_recovery(Lexer *lx) {
  char c = lx_current(lx);
  return recovery_states[c];
}

StateFn state_sign(Lexer *lx) {
  char c = lx_next(lx);
  if (isdigit(c)) {
    return (StateFn){state_digits};
  } else {
    return (StateFn){state_error};
  }
}

static const StateFn start_map[256] = {
    ['"'] = (StateFn){state_dq_string},
    ['\''] = (StateFn){state_sq_string},
    ['0'] = (StateFn){state_zero},
    ['1' ... '9'] = (StateFn){state_digits},
    ['-'] = (StateFn){state_sign},
    ['{'] = (StateFn){state_lcurly},
    ['}'] = (StateFn){state_rcurly},
    ['['] = (StateFn){state_lsquare},
    [']'] = (StateFn){state_rsquare},
    [','] = (StateFn){state_comma},
    [':'] = (StateFn){state_colon},
    ['a' ... 'z'] = (StateFn){state_keyword},
    [' '] = (StateFn){state_ignore},
    ['\t'] = (StateFn){state_ignore},
    ['\r'] = (StateFn){state_ignore},
    ['\n'] = (StateFn){state_ignore},
};
StateFn state_start(Lexer *lx) {
  char c = lx_current(lx);
  return start_map[c];
};

static const StateFn ignore_map[256] = {
    [0 ... 255] = (StateFn){state_start}, [' '] = (StateFn){state_ignore},
    ['\t'] = (StateFn){state_ignore},     ['\r'] = (StateFn){state_ignore},
    ['\n'] = (StateFn){state_ignore},
};
StateFn state_ignore(Lexer *lx) {
  char c = lx_next(lx);
  lx_ignore(lx);
  return ignore_map[c];
};

StateFn state_zero(Lexer *lx) {
  char c = lx_next(lx);
  if (isdigit(c))
    return (StateFn){state_error};
  if (c == '.')
    return (StateFn){state_float};
  else {
    lx_emit(lx, TypeInteger);
    return (StateFn){state_recovery};
  }
};
