#ifndef DEBUG_H
#define DEBUG_H

typedef struct Token Token;
typedef struct Astnode Astnode;

void dbg_print_tokens(Token** tokens);
void dbg_print_ast(Astnode** ast);

#endif
