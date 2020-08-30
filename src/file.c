#include <file.h>
#include <arpch.h>

#define _TAB_COUNT 4

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

char* get_line_in_file(File* srcfile, u64 line) {
	char* line_start = srcfile->contents;
	u64 line_idx = 1;
	while (line_idx != line) {
		while (*line_start != '\n') {
			if (*line_start == '\0') {
				return null;
			}
			else line_start++;
		}
		line_start++;
		line_idx++;
	}
	return line_start;
}

void print_file_line(File* srcfile, u64 line) {
	char* line_start = get_line_in_file(srcfile, line);
	while (*line_start != '\n' && *line_start != '\0') {
		if (*line_start == '\t') print_tab();
		else fprintf(stderr, "%c", *line_start);
		line_start++;
	}
	fprintf(stderr, "\n");
}

void print_tab(void) {
	for (u64 t = 0; t < _TAB_COUNT; t++) {
		fprintf(stderr, " ");
	}
}
