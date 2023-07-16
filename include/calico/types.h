#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

typedef uint8_t  u8;       ///<  8-bit unsigned integer.
typedef uint16_t u16;      ///< 16-bit unsigned integer.
typedef uint32_t u32;      ///< 32-bit unsigned integer.
typedef uint64_t u64;      ///< 64-bit unsigned integer.
typedef uintptr_t uptr;    ///< Pointer-sized unsigned integer.

typedef int8_t  s8;        ///<  8-bit signed integer.
typedef int16_t s16;       ///< 16-bit signed integer.
typedef int32_t s32;       ///< 32-bit signed integer.
typedef int64_t s64;       ///< 64-bit signed integer.
typedef intptr_t sptr;     ///< Pointer-sized signed integer.

typedef volatile u8  vu8;  ///<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; ///< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; ///< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; ///< 64-bit volatile unsigned integer.
typedef volatile uptr vuptr; ///< Pointer-sized volatile unsigned integer.

typedef volatile s8  vs8;  ///<  8-bit volatile signed integer.
typedef volatile s16 vs16; ///< 16-bit volatile signed integer.
typedef volatile s32 vs32; ///< 32-bit volatile signed integer.
typedef volatile s64 vs64; ///< 64-bit volatile signed integer.
typedef volatile sptr vsptr; ///< Pointer-sized volatile signed integer.

#define MK_PACKED     __attribute__((packed))
#define MK_DEPRECATED __attribute__((deprecated))
#define MK_INLINE     __attribute__((always_inline)) static inline
#define MK_NOINLINE   __attribute__((noinline))
#define MK_NORETURN   __attribute__((noreturn))
#define MK_PURE       __attribute__((pure))
#define MK_WEAK       __attribute__((weak))
#define MK_DUMMY(_x)  (void)(_x)

#if defined(__cplusplus)
#define MK_EXTERN_C       extern "C"
#define MK_EXTERN_C_START MK_EXTERN_C {
#define MK_EXTERN_C_END   }
#else
#define MK_EXTERN_C
#define MK_EXTERN_C_START
#define MK_EXTERN_C_END
#endif

#if __thumb__
#define MK_CODE32 __attribute__((target("arm")))
#else
#define MK_CODE32
#endif

#if __thumb__ && __ARM_ARCH < 5
#define MK_EXTERN32   __attribute__((long_call))
#else
#define MK_EXTERN32
#endif

#if __cplusplus >= 201402L
#define MK_CONSTEXPR  MK_INLINE constexpr
#else
#define MK_CONSTEXPR  MK_INLINE
#endif

#if __cplusplus >= 201103L
#define MK_STRUCT_ALIGN(_align) alignas(_align)
#else
#define MK_STRUCT_ALIGN(_align) __attribute__((aligned(_align)))
#endif

#define MK_REG(_type,_off) (*(_type volatile*)(MM_IO + (_off)))

#define if_likely(_expr)   if(__builtin_expect(!!(_expr), 1))
#define if_unlikely(_expr) if(__builtin_expect(!!(_expr), 0))

#define MK_ASSUME(_expr) do { if (!(_expr)) __builtin_unreachable(); } while (0)
