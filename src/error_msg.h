#ifndef _ERROR_MSG_H
#define _ERROR_MSG_H

#include "arpch.h"
#include "util/util.h"
#include "ds/ds.h"

typedef enum {
    MSG_ERROR,
    MSG_NOTE,
} MsgType;

void msg(
    MsgType type,
	File* srcfile,
	u64 line,
	u64 column,
	u64 char_count,
	const char* fmt,
	va_list ap);

void msg_token(
        MsgType type,
        Token* token,
        const char* fmt,
        ...);
void vmsg_token(
        MsgType type,
        Token* token,
        const char* fmt,
        va_list ap);

void msg_data_type(
        MsgType type,
        DataType* dt,
        const char* fmt,
        ...);
void vmsg_data_type(
        MsgType type,
        DataType* dt,
        const char* fmt,
        va_list ap);

void msg_expr(
        MsgType type,
        Expr* expr,
        const char* fmt,
        ...);
void vmsg_expr(
        MsgType type,
        Expr* expr,
        const char* fmt,
        va_list ap);

void error_info_expect_type(DataType* dt);
void error_info_got_type(DataType* dt);

void error_info_expect_u64(char* pre, u64 n);
void error_info_got_u64(char* pre, u64 n);

void error_common(const char* fmt, ...);
void fatal_error_common(const char* fmt, ...);

#ifndef _MSG_NO_ERROR_MACRO
#define error_token(token, ...) \
    self->error_state = true, \
    self->error_count++, \
    msg_token(MSG_ERROR, token, ##__VA_ARGS__)

#define error_data_type(dt, ...) \
    self->error_state = true, \
    self->error_count++, \
    msg_data_type(MSG_ERROR, dt, ##__VA_ARGS__)

#define error_expr(expr, ...) \
    self->error_state = true, \
    self->error_count++, \
    msg_expr(MSG_ERROR, expr, ##__VA_ARGS__)

#define note_token(token, ...) \
    msg_token(MSG_NOTE, token, ##__VA_ARGS__)

#define note_data_type(dt, ...) \
    msg_data_type(MSG_NOTE, dt, ##__VA_ARGS__)

#define note_expr(expr, ...) \
    msg_expr(MSG_NOTE, expr, ##__VA_ARGS__)
#endif

#endif /* _ERROR_MSG_H */
