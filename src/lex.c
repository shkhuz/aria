#include "aria.h"
#include "arpch.h"

bool lex(Lexer* self, File* srcfile) {
    self->srcfile = srcfile;
    self->tokens =  null;
	self->start = srcfile->contents;
	self->current = self->start;
	self->line = 1;
	self->last_newline = self->start;
    self->error_state = false;
}

