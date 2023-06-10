#pragma once
#include <calico/types.h>
#include <calico/dev/mwl.h>

#define MWL_DEBUG

#ifdef MWL_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif

unsigned _mwlBbpRead(unsigned reg);
void _mwlBbpWrite(unsigned reg, unsigned value);
void _mwlRfCmd(u32 cmd);
