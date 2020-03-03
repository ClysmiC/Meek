#pragma once

#ifdef _MSC_VER
    #define DEBUG _DEBUG
    #define _CRT_SECURE_NO_DEPRECATE
#endif

#if DEBUG
    #define ALS_DEBUG
#endif

#include "macro.h"
#include "common.h"


// Use this to document pointers in struct definitions and function declarations that are allowed to be null.
//	If this is omitted, you can assume the pointer is never null after initialization.
//	This is an experiment, since I am considering a similar feature for my language.

#define NULLABLE

// Flip this on if you need to debug heap corruption!

#define DEBUG_HEAP_CORRUPTION 0
#if DEBUG_HEAP_CORRUPTION
#include <crtdbg.h>
#endif
