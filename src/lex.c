#include "lex.h"

LexContext lex_new_context(Srcfile* srcfile) {
    LexContext l;
    l.srcfile = srcfile;
    l.start = srcfile->handle.contents;
    l.current = l.start;
    l.last_newl = l.start;
    l.line = 1;
    l.error = false;
    return l;
}
