#pragma once

// How to use these headers:

// -Any header on its own should be a portable drop-in with no dependencies
//  OR
// -Include "common.h" to get the whole "common_X" suite
//  OR
// -Include "macro.h" to get the whole "macro_X" suite
//  OR
// -Include "macro.h" and then "common.h" into a common header of your choosing to include both
//  They still work just fine in the opposite order.
// -Define ALS_DEBUG before any header to get debug versions

// Caveat: There are almost certainly portability bugs. These headers haven't been tested on
//  a variety of setups

#ifndef ALS_COMMON
    #define ALS_COMMON
#endif

#include "common_alloc.h"
#include "common_array.h"
#include "common_hash.h"
#include "common_sort.h"
#include "common_string.h"
#include "common_type.h"
