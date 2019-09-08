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

// Flip this on if you need to debug heap corruption!

#define DEBUG_HEAP_CORRUPTION 0
#if DEBUG_HEAP_CORRUPTION
#include <crtdbg.h>
#endif
