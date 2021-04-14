#include <aria_core.h>
#include <aria.h>

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
	///// TESTS END /////	

	if (argc < 2) {
		aria_error("aria: one or more input files needed");
		aria_terminate();
	}

	// `File` is a regular I/O file
	// `SrcFile` is a wrapper for the `File`,
	// and the parsing context (symbol table, imports, etc)
	SrcFile** srcfiles = null;
	bool srcfile_error = false;
	for (int i = 1; i < argc; i++) {
		File* file = file_read(argv[i]);
		if (file) {
			alloc_with_type(srcfile, SrcFile*);
			srcfile->contents = file;
			buf_push(srcfiles, srcfile);
		} else {
			aria_error(ERROR_CANNOT_READ_SOURCE_FILE, argv[i]);
			srcfile_error = true;
		}
	}

	if (srcfile_error) {
		aria_terminate();
	}	
}
