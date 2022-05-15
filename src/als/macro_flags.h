#pragma once

// Usage:
//  ForFlag(FFlag, fflag)
//  {
//      ...

#define ForFlag(Flag, flag) for(Flag flag = Flag##_LowestFlag ; flag <= Flag##_HighestFlag ; flag = (Flag)(flag << 1)) 