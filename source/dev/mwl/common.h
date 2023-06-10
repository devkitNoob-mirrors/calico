#pragma once
#include <calico/types.h>
#include <calico/dev/mwl.h>

#define MWL_DEBUG

#ifdef MWL_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif
