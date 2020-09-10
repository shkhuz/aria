#include <file.h>
#include <arpch.h>

File* file_read(const char* fpath) {
	/* TODO: more thorough error checking */
	FILE* fp = fopen(fpath, "r");
	if (!fp) return null;
	fseek(fp, 0, SEEK_END);
	u64 size = (u64)ftell(fp);
	rewind(fp);

	char* contents = (char*)malloc(size + 1);
	fread((void*)contents, sizeof(char), size, fp);
	fclose(fp);

	File* file = malloc(sizeof(File));
	file->fpath = (char*)fpath;
	file->contents = contents;
	file->len = size;
	return file;
}

bool file_exists(const char* fpath) {
	FILE* fp = fopen(fpath, "r");
	if (fp) return true;
	fclose(fp);
	return false;
}

