#pragma once

// How to use these headers:

// -Any header on its own should be a portable drop-in with no dependencies
//  OR
// -Include "common.h" to get the whole common_ suite
//  OR
// -Include "macro.h" to get the whole macro_ suite
//  OR
// -Include "macro.h" and then "common.h" into a common header of your choosing to include both
//  They still work just fine in the opposite order.
// -Define ALS_DEBUG before any header to get debug versions

// Caveat: There are almost certainly portability bugs. These headers haven't been tested on
//  a variety of setups

#ifndef ALS_MACRO
    #define ALS_MACRO
#endif

#include "macro_assert.h"
#include "macro_defer.h"
#include "macro_flags.h"
#include "macro_mem.h"
#include "macro_util.h"
