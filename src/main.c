#include <aria_core.h>
#include <aria.h>

char* executable_path_from_argv;

// --- NOTE: update KEYWORDS_LEN after adding to keywords ---
char* keywords[KEYWORDS_LEN] = {
	"namespace",
	"struct",
	"fn",
	"let",
	"mut",
	"const",
	"_",
};

// --- NOTE: update DIRECTIVES_LEN after adding to directives ---
char* directives[DIRECTIVES_LEN] = {
	"import",
};

char* aria_strsub(char* str, uint start, uint len) {
	if (!str) {
		return null;
	}

	if (len == 0) {
		return null;
	}

	char* res = null;
	for (uint i = start; i < start + len; i++) {
		buf_push(res, str[i]);
	}
	buf_push(res, '\0');
	return res;
}

char* aria_strapp(char* to, char* from) {
	if (!to || !from) {
		return null;
	}

	char* res = null;
	uint to_len = strlen(to);
	uint from_len = strlen(from);
	for (uint i = 0; i < to_len; i++) {
		buf_push(res, to[i]);
	}
	for (uint i = 0; i < from_len; i++) {
		buf_push(res, from[i]);
	}
	buf_push(res, '\0');
	return res;
}

char* aria_basename(char* fpath) {
	if (!fpath) {
		return null;
	} 

	char* last_dot_ptr = strrchr(fpath, '.');
	if (last_dot_ptr == null) {
		return fpath;
	}

	return aria_strsub(fpath, 0, last_dot_ptr - fpath);
}

char* aria_notdir(char* fpath) {
	if (!fpath) {
		return null;
	} 

	uint fpath_len = strlen(fpath);
	char* last_slash_ptr = strrchr(fpath, '/');
	if (last_slash_ptr == null) {
		return fpath;
	}

	return aria_strsub(fpath, last_slash_ptr - fpath + 1, fpath_len);
}	

bool parse_srcfile(SrcFile* srcfile) {
	Lexer lexer;
	lexer_init(&lexer, srcfile); 
	lexer_lex(&lexer);
	if (lexer.error) {
		return true;
	}

	// Lexer tokens check
	Token** tokens = srcfile->tokens;
	/* buf_loop(tokens, j) { */
	/* 	printf("[%u, %lu:%lu:%lu] ", tokens[j]->ty, tokens[j]->line, tokens[j]->column, tokens[j]->char_count); */
	/* 	for (char* start = tokens[j]->start; start != tokens[j]->end; start++) { */
	/* 		printf("%c", *start); */
	/* 	} */
	/* 	printf("\n"); */
	/* } */

	Parser parser;
	parser_init(&parser, srcfile);
	parser_parse(&parser);
	if (parser.error) {
		return true;
	}

	AstPrinter ast_printer;
	ast_printer_init(&ast_printer, srcfile);
	ast_printer_print(&ast_printer);
	return false;
}

bool parse_srcfiles(SrcFile** srcfiles) {
	bool error = false;
	buf_loop(srcfiles, i) {
		bool current_error = parse_srcfile(srcfiles[i]);
		if (!error) {
			error = current_error;
		}
	}
	return error;
}

bool check_srcfile(SrcFile* srcfile) {
	Resolver resolver;
	resolver_init(&resolver, srcfile);
	resolver_resolve(&resolver);
	if (resolver.error) {
		return true;
	}
	return false;
}

bool check_srcfiles(SrcFile** srcfiles) {
	bool error = false;
	buf_loop(srcfiles, i) {
		bool current_error = check_srcfile(srcfiles[i]);
		if (!error) {
			error = current_error;
		}
	}
	return error;
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
		msg_user(MSG_TY_ROOT_ERR, null, 0, 0, 0, ERROR_NO_SOURCE_FILES_PASSED_IN_CMD_ARGS);
		terminate_compilation();
	}

	// `File` is a regular I/O file
	// `SrcFile` is a wrapper for the `File`,
	// and the parsing context (symbol table, imports, etc)
	SrcFile** srcfiles = null;
	bool srcfile_error = false;
	for (int i = 1; i < argc; i++) {
		FileOrError file = file_read(argv[i]);
		if (file.status == FILE_ERROR_SUCCESS) {
			alloc_with_type(srcfile, SrcFile);
			srcfile->contents = file.file;
			buf_push(srcfiles, srcfile);
		} else if (file.status == FILE_ERROR_ERROR) {
			msg_user(MSG_TY_ROOT_ERR, null, 0, 0, 0, ERROR_CANNOT_READ_SOURCE_FILE, argv[i]);
			srcfile_error = true;
		} else if (file.status == FILE_ERROR_DIR) {
			msg_user(MSG_TY_ROOT_ERR, null, 0, 0, 0, ERROR_SOURCE_FILE_IS_DIRECTORY, argv[i]);
			srcfile_error = true;
		}
	}

	if (srcfile_error) {
		terminate_compilation();
	}	

	bool parsing_error = parse_srcfiles(srcfiles);
	if (parsing_error) {
		terminate_compilation();
	}

	bool import_files_error = false;
	buf_loop(srcfiles, s) {
		buf_loop(srcfiles[s]->imports, i) {
			bool parsed = false;
			buf_loop(srcfiles, ss) {
				if (stri(srcfiles[s]->imports[i].srcfile->contents->abs_fpath) == stri(srcfiles[ss]->contents->abs_fpath)) {
					parsed = true;
				}
			}

			if (parsed) {
				continue;
			}

			bool current_import_file_error = parse_srcfile(srcfiles[s]->imports[i].srcfile);
			if (!current_import_file_error) {
				buf_push(srcfiles, srcfiles[s]->imports[i].srcfile);
			} else {
				import_files_error = true;
			}
		}
	}

	if (import_files_error) {
		terminate_compilation();
	}

	bool checking_error = check_srcfiles(srcfiles);
	if (checking_error) {
		terminate_compilation();
	}
}
