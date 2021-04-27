#include <aria_core.h>
#include <aria.h>

Token* token_alloc(
		TokenType ty, 
		char* lexeme, 
		char* start, 
		char* end, 
		SrcFile* srcfile, 
		u64 line, 
		u64 column, 
		u64 char_count) {
	
	alloc_with_type(token, Token);
	*token = (Token) {
		ty,
		lexeme,
		start,
		end,
		srcfile,
		line,
		column,
		char_count
	};
	return token;
}

bool token_lexeme_eql(Token* a, Token* b) {
	if (stri(a->lexeme) == stri(b->lexeme)) {
		return true;
	}
	return false;
}
