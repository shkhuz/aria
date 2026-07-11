#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"

void dbg_print_tokens(Token** tokens, Srcfile* src);
void dbg_print_ast(Astnode** ast, Srcfile* src);

#endif
