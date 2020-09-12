#ifndef _ARPCH_H
#define _ARPCH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>

#include <types.h>
#include <math.h>
#include <buf.h>
#include <str_intern.h>
#include <file.h>

#define COMBINE1(X, Y) X##Y
#define COMBINE(X,Y) COMBINE1(X,Y)

#define ANSI_FBOLD    "\x1B[1m"
#define ANSI_FRED     "\x1B[1;31m"
/* #define ANSI_FGREEN   "\x1B[32m" */
/* #define ANSI_FYELLOW  "\x1B[33m" */
/* #define ANSI_FBLUE    "\x1B[34m" */
#define ANSI_FMAGENTA "\x1B[35m"
/* #define ANSI_FCYAN    "\x1B[36m" */
#define ANSI_RESET   "\x1B[0m"

#endif /* _ARPCH_H */
