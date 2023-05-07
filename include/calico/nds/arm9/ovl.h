#pragma once
#include "../../types.h"
#include "../env.h"

MEOW_EXTERN_C_START

typedef EnvNdsOverlay OvlParams;
typedef void (*OvlStaticFn)(void);

bool ovlInit(void);

bool ovlLoadInPlace(unsigned ovl_id);

void ovlActivate(unsigned ovl_id);

void ovlDeactivate(unsigned ovl_id);

MEOW_INLINE bool ovlLoadAndActivate(unsigned ovl_id)
{
	bool ok = ovlLoadInPlace(ovl_id);
	if (ok) {
		ovlActivate(ovl_id);
	}
	return ok;
}

MEOW_EXTERN_C_END
