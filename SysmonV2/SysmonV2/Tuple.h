// Tuple.cpp
// Kernel polyfill for std::tuple.

#pragma once

template <typename T, typename U>
class Tuple
{
public:
	Tuple(T first, U second)
		: m_First(first), m_Second(second) {};

	~Tuple() = default;

	T First() const
	{
		return m_First;
	}

	U Second() const
	{
		return m_Second;
	}

private:
	T m_First;
	U m_Second;
};
