#pragma once
#if !defined(__NDS__) || !defined(ARM7)
#error "This header file is only for NDS ARM7"
#endif

#include "../../types.h"

typedef enum DebugBufferMode {
	DbgBufMode_None = 0,
	DbgBufMode_Line = 1,
	DbgBufMode_Full = 2,
} DebugBufferMode;

MEOW_INLINE void debugSetBufferMode(DebugBufferMode mode) {
	extern DebugBufferMode g_dbgBufMode;
	g_dbgBufMode = mode;
}

void debugSetupStreams(void);
void debugOutput(const char* buf, size_t size);
