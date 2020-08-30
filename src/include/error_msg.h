#ifndef _ERROR_MSG_H
#define _ERROR_MSG_H

#include <arpch.h>

void error(
	File* srcfile,
	u64 line,
	u64 column,
	u64 char_count,
	const char* fmt,
	va_list ap);

#endif /* _ERROR_MSG_H */
