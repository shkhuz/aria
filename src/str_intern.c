#include <aria_core.h>

static StrIntern* interns;

char* strni(char* start, char* end) {
	u64 len = end - start;
	buf_loop(interns, i) {
		if (interns[i].len == len &&
			strncmp(interns[i].str, start, len) == false) {
			return interns[i].str;
		}
	}

	char* str = (char*)malloc(len + 1);
	memcpy(str, start, len);
	str[len] = 0;
	buf_push(interns, (StrIntern){ str, len });
	return str;
}

char* stri(char* str) {
	return strni(str, str + strlen(str));
}
