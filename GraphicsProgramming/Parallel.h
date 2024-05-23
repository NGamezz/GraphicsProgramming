#pragma once

#include <functional>
#include <thread>

class Parallel
{
public:
	static void parallelFor(const int start, const int end, std::function<void(int)> body);

};