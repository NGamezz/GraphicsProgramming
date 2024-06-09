#pragma once

#include <functional>
#include <thread>
#include "ThreadPool.h"

class Parallel
{
public:
	static void parallelFor(const int start, const int end, const std::function<void(int)> body, ThreadPool& threadPool);

};