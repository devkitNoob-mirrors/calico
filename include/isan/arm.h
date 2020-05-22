#pragma once

#define ARM_PSR_MODE_USR  0x10
#define ARM_PSR_MODE_FIQ  0x11
#define ARM_PSR_MODE_IRQ  0x12
#define ARM_PSR_MODE_SVC  0x13
#define ARM_PSR_MODE_ABT  0x17
#define ARM_PSR_MODE_UND  0x1b
#define ARM_PSR_MODE_SYS  0x1f
#define ARM_PSR_MODE_MASK 0x1f

#define ARM_PSR_T (1<<5)
#define ARM_PSR_F (1<<6)
#define ARM_PSR_I (1<<7)
#define ARM_PSR_Q (1<<27)
#define ARM_PSR_V (1<<28)
#define ARM_PSR_C (1<<29)
#define ARM_PSR_Z (1<<30)
#define ARM_PSR_N (1<<31)
