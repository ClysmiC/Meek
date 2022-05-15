#pragma once

// Courtesy of https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/

template <typename F>
struct _defer {
	F f;
	_defer(F f) : f(f) {}
	~_defer() { f(); }
};

template <typename F>
_defer<F> _defer_func(F f) {
	return _defer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)	  DEFER_2(x, __COUNTER__)
#define Defer(code)	  auto DEFER_3(_defer_) = _defer_func([&](){code;})
