#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "imq.h"

void imq_log(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
