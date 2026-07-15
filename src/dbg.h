#ifndef DEBUG_H
#define DEBUG_H

#include "types.h"
#include "parse.h"

void dbg_print_tokens(Srcfile* src);
void dbg_nodes(ParseCtx* p);

#endif
