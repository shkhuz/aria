#ifndef _ERROR_MSG_H
#define _ERROR_MSG_H

#include "arpch.h"
#include "util/util.h"
#include "ds/ds.h"

void error(
	File* srcfile,
	u64 line,
	u64 column,
	u64 char_count,
	const char* fmt,
	va_list ap);

void error_token(
        Token* token,
        const char* fmt,
        ...);
void verror_token(
        Token* token,
        const char* fmt,
        va_list ap);

void error_data_type(
        DataType* dt,
        const char* fmt,
        ...);
void verror_data_type(
        DataType* dt,
        const char* fmt,
        va_list ap);

void error_expr(
        Expr* expr,
        const char* fmt,
        ...);
void verror_expr(
        Expr* expr,
        const char* fmt,
        va_list ap);

void error_info_expect_type(DataType* dt);
void error_info_got_type(DataType* dt);

void error_common(const char* fmt, ...);
void fatal_error_common(const char* fmt, ...);

#ifndef _ERROR_MSG_NO_ERROR_MACRO
#define error_token(token, ...) \
    self->error_state = true, \
    error_token(token, ##__VA_ARGS__)

#define error_data_type(dt, ...) \
    self->error_state = true, \
    error_data_type(dt, ##__VA_ARGS__)

#define error_expr(expr, ...) \
    self->error_state = true, \
    error_expr(expr, ##__VA_ARGS__)
#endif

#endif /* _ERROR_MSG_H */
