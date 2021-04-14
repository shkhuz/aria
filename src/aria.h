#ifndef __ARIA_H
#define __ARIA_H

///// TYPES /////
typedef struct {
	File* contents;
} SrcFile;

///// LEXER /////
typedef struct {
	SrcFile* srcfile;
} Lexer;

void lexer_init(Lexer* self, SrcFile* srcfile);

#endif	/* __ARIA_H */
