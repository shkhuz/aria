#include <aria_core.h>
#include <aria.h>

Token* token_alloc(TokenType ty, char* start, char* end) {
	alloc_with_type(token, Token);
	*token = (Token) {
		ty,
		start,
		end,
	};
	return token;
}
