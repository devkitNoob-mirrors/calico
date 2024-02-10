#pragma once
#include "../types.h"
#include "thread.h"
#include "mutex.h"

/*! @addtogroup sync
	@{
*/
/*! @name Condition variable
	@{
*/

MK_EXTERN_C_START

typedef struct CondVar {
	u8 dummy;
} CondVar;

void condvarSignal(CondVar* cv);
void condvarBroadcast(CondVar* cv);
void condvarWait(CondVar* cv, Mutex* m);

MK_EXTERN_C_END

//! @}

//! @}
