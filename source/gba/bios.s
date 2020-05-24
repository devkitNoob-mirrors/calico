#include <isan/asm.inc>

.macro BIOSFUNC id, name
FUNC_START16 svc\name
	svc \id
	bx  lr
FUNC_END
.endm

BIOSFUNC 0x00, SoftReset
BIOSFUNC 0x01, RegisterRamReset
BIOSFUNC 0x02, Halt
BIOSFUNC 0x03, Stop
BIOSFUNC 0x04, IntrWait
BIOSFUNC 0x05, VBlankIntrWait
BIOSFUNC 0x06, Div
BIOSFUNC 0x07, DivArm
BIOSFUNC 0x08, Sqrt
BIOSFUNC 0x09, ArcTan
BIOSFUNC 0x0a, ArcTan2
BIOSFUNC 0x0b, CpuSet
BIOSFUNC 0x0c, CpuFastSet
BIOSFUNC 0x0d, GetBiosChecksum
BIOSFUNC 0x0e, BgAffineSet
BIOSFUNC 0x0f, ObjAffineSet
BIOSFUNC 0x10, BitUnpack
BIOSFUNC 0x11, LZ77UncompWram
BIOSFUNC 0x12, LZ77UncompVram
BIOSFUNC 0x13, HuffUncomp
BIOSFUNC 0x14, RLUncompWram
BIOSFUNC 0x15, RLUncompVram
BIOSFUNC 0x16, Diff8bitUnfilterWram
BIOSFUNC 0x17, Diff8bitUnfilterVram
BIOSFUNC 0x18, Diff16bitUnfilter
BIOSFUNC 0x19, SoundBias
BIOSFUNC 0x1f, MidiKey2Freq
BIOSFUNC 0x25, MultiBoot
BIOSFUNC 0x26, HardReset
