#include <aria_core.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static int is_dir(const char* fpath) {
	struct stat fpath_stat;
	stat(fpath, &fpath_stat);
	return S_ISDIR(fpath_stat.st_mode);
}

FileOrError file_read(const char* fpath) {
	/* TODO: more thorough error checking */
	FILE* fp = fopen(fpath, "r");
	if (!fp) {
		return (FileOrError){ null, FILE_ERROR_ERROR };
	}

	if (is_dir(fpath)) {
		return (FileOrError){ null, FILE_ERROR_DIR };
	}

	fseek(fp, 0, SEEK_END);
	u64 size = ftell(fp);
	rewind(fp);

	char* contents = (char*)malloc(size + 1);
	fread((void*)contents, sizeof(char), size, fp);
	fclose(fp);

	char abs_fpath[PATH_MAX+1];
	char* abs_fpath_ptr = realpath(fpath, abs_fpath);
	if (!abs_fpath_ptr) {
		return (FileOrError){ null, FILE_ERROR_ERROR };
	}

	File* file = malloc(sizeof(File));
	file->fpath = stri((char*)fpath);
	file->abs_fpath = stri(abs_fpath_ptr);
	file->contents = contents;
	file->len = size;
	return (FileOrError){ file, FILE_ERROR_SUCCESS };
}

bool file_exists(const char* fpath) {
	FILE* fp = fopen(fpath, "r");
	if (fp) {
		fclose(fp);
		return true;
	}
	return false;
}
