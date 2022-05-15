#pragma once

#include "string.h"

#define ClearStruct(pStruct) memset(pStruct, 0, sizeof(*(pStruct)))

// TODO: move this to macro_array ??

#define ArrayLen(arr) sizeof(arr) / sizeof(arr[0])