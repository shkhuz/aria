#include <aria_core.h>
#include <aria.h>

char* executable_path_from_argv;

void parse_srcfiles(SrcFile** srcfiles) {
	buf_loop(srcfiles, i) {
		Lexer lexer;
		lexer_init(&lexer, srcfiles[i]); 
		lexer_lex(&lexer);

		// Lexer test
		Token** tokens = srcfiles[i]->tokens;
		buf_loop(tokens, j) {
			printf("[%u] ", tokens[j]->ty);
			for (char* start = tokens[j]->start; start != tokens[j]->end; start++) {
				printf("%c", *start);
			}
			printf("\n");
		}
		assert(strni(tokens[0]->start, tokens[0]->end) == stri("hello"));
		assert(strni(tokens[1]->start, tokens[1]->end) == stri("_hello"));
		assert(strni(tokens[2]->start, tokens[2]->end) == stri("_"));
		assert(strni(tokens[3]->start, tokens[3]->end) == stri("_123"));
		assert(buf_len(tokens) == 4);
	}
}

int main(int argc, char* argv[]) {
	///// TESTS /////
	{
		u8* buffer = null;
		
		buf_push(buffer, 1);
		buf_push(buffer, 2);
		buf_push(buffer, 3);
		
		assert(buffer[0] == 1);
		assert(buffer[1] == 2);
		assert(buffer[2] == 3);

		assert(buf_len(buffer) == 3);
		buf_remove(buffer, 2);
		assert(buf_len(buffer) == 2);
		buf_remove(buffer, 1);
		assert(buf_len(buffer) == 1);
		buf_remove(buffer, 0);
		assert(buf_len(buffer) == 0);
	}

	{
		char* string1 = "hello";
		char* string2 = "hello";
		assert(stri(string1) == stri(string2));

		char* string3 = "Hello";
		assert(stri(string1) != stri(string3));

		char* empty = "";
		assert(stri("") == stri(empty));
	}

	{
		alloc_with_type(i, u8);
		free(i);	
	}
	///// TESTS END /////	
	executable_path_from_argv = argv[0];

	if (argc < 2) {
		msg_user(MSG_TY_ROOT, 0, "one or more input files needed");
		terminate_compilation();
	}

	// `File` is a regular I/O file
	// `SrcFile` is a wrapper for the `File`,
	// and the parsing context (symbol table, imports, etc)
	SrcFile** srcfiles = null;
	bool srcfile_error = false;
	for (int i = 1; i < argc; i++) {
		File* file = file_read(argv[i]);
		if (file) {
			alloc_with_type(srcfile, SrcFile);
			srcfile->contents = file;
			buf_push(srcfiles, srcfile);
		} else {
			msg_user(MSG_TY_ERR, ERROR_CANNOT_READ_SOURCE_FILE, argv[i]);
			srcfile_error = true;
		}
	}

	if (srcfile_error) {
		terminate_compilation();
	}	

	parse_srcfiles(srcfiles);
}
