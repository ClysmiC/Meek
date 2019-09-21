#pragma once

#include <stdint.h>

typedef int8_t      s8;
typedef int16_t     s16;
typedef int32_t     s32;
typedef int64_t     s64;


typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef float       f32;
typedef double      f64;

typedef unsigned int uint;

#define S32_MAX  2147483647

// Tricky -1 is necessary because -2147483648 gets treated as an unsigned long literal with a unary operator...

#define S32_MIN (-2147483647 - 1)