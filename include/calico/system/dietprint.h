#pragma once
#include <stdarg.h>
#include "../types.h"

MK_EXTERN_C_START

typedef void (*DietPrintFn)(const char* buf, size_t size);

MK_INLINE void dietPrintSetFunc(DietPrintFn fn)
{
	extern DietPrintFn g_dietPrintFn;
	g_dietPrintFn = fn;
}

void dietPrint(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void dietPrintV(const char* fmt, va_list va) __attribute__((format(printf, 1, 0)));

MK_EXTERN_C_END
