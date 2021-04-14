#include <aria_core.h>
#include <aria.h>

void lexer_init(Lexer* self, SrcFile* srcfile) {
	self->srcfile = srcfile;
}
