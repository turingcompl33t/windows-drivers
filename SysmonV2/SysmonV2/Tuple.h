// Tuple.cpp
// Kernel polyfill for std::tuple.

#pragma once

template <typename T, typename U>
class Tuple
{
public:
	Tuple(T first, U second)
		: First(first), Second(second) {};

	~Tuple() = default;

	T First() const;
	U Second() const;

private:
	T First;
	U Second;
};
