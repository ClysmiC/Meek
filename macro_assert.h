#pragma once

#define Assert(x) do { if (!(x))  __debugbreak(); } while(0)