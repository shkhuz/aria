#ifndef _FILE_H
#define _FILE_H

#include <types.h>

typedef struct {
	char* fpath;
	char* contents;
	u64 len;
} File;

File* file_read(const char* fpath);
bool file_exists(const char* fpath);
char* get_line_in_file(File* srcfile, u64 line);
void print_file_line(File* srcfile, u64 line);
void print_tab(void);

#endif /* _FILE_H */
