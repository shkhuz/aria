#ifndef _DS_H
#define _DS_H

#include "token.h"
#include "token_type.h"
#include "data_type.h"
#include "builtin_types.h"
#include "expr.h"
#include "stmt.h"
#include "ast.h"

extern const char** keywords;

void keywords_init(void);

#endif /* _DS_H */
