#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdalign.h>

typedef uint8_t  u8;       ///<  8-bit unsigned integer.
typedef uint16_t u16;      ///< 16-bit unsigned integer.
typedef uint32_t u32;      ///< 32-bit unsigned integer.
typedef uint64_t u64;      ///< 64-bit unsigned integer.

typedef int8_t  s8;        ///<  8-bit signed integer.
typedef int16_t s16;       ///< 16-bit signed integer.
typedef int32_t s32;       ///< 32-bit signed integer.
typedef int64_t s64;       ///< 64-bit signed integer.

typedef volatile u8  vu8;  ///<  8-bit volatile unsigned integer.
typedef volatile u16 vu16; ///< 16-bit volatile unsigned integer.
typedef volatile u32 vu32; ///< 32-bit volatile unsigned integer.
typedef volatile u64 vu64; ///< 64-bit volatile unsigned integer.

typedef volatile s8  vs8;  ///<  8-bit volatile signed integer.
typedef volatile s16 vs16; ///< 16-bit volatile signed integer.
typedef volatile s32 vs32; ///< 32-bit volatile signed integer.
typedef volatile s64 vs64; ///< 64-bit volatile signed integer.

#define IS_PACKED     __attribute__((packed))
#define IS_DEPRECATED __attribute__((deprecated))
#define IS_INLINE     __attribute__((always_inline)) static inline
#define IS_NORETURN   __attribute__((noreturn))
#define IS_DUMMY(_x)  (void)(_x)

#if __cplusplus >= 201402L
#define IS_CONSTEXPR  IS_INLINE constexpr
#else
#define IS_CONSTEXPR  IS_INLINE
#endif

#define IS_REG(_type,_off) (*(_type volatile*)(MM_IO + (_off)))
