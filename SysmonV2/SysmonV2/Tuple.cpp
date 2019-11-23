// Tuple.cpp
// Kernel polyfill for std::tuple.

#include "Tuple.h"

template<typename T, typename U>
T Tuple<T, U>::First() const
{
	return First;
}

template<typename T, typename U>
U Tuple<T, U>::Second() const
{
	return Second;
}

