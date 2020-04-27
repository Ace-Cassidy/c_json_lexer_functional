#include "lex/lex.h"

int main(int argc, char const *argv[]) {
  FILE *f = fopen(argv[1], "r");
  Lexer *lx = lex(f);

  void *msg;
  Token *tkn;
  while (chan_recv(lx->emitter, &msg) == 0) {
    tkn = (Token *)msg;
    printf("Recieved Token: .type=%c, .val=%.*s \n", tkn->type,
           tkn->end - tkn->start + 1, lx->input_text + tkn->start);
  }
}
