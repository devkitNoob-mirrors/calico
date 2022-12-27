#pragma once
#include <calico/types.h>
#include <calico/dev/sdio.h>
#include <calico/dev/ar6k.h>

#define AR6K_DEBUG

#ifdef AR6K_DEBUG
#include <calico/system/dietprint.h>
#else
#define dietPrint(...) ((void)0)
#endif
