typedef struct {
	SrcFile* srcfile;
	u64 token_idx;
	bool in_function;
	bool error;
} Parser;
