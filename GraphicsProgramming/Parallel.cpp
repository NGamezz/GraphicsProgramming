#include "Parallel.h"

void Parallel::parallelFor(const int start, const int end, std::function<void(int)> body)
{
	std::vector<std::thread> threads;

	for (int i = start; i < end; i++)
	{
		threads.emplace_back([&]() {body(i); });
	}

	for (auto& thread : threads)
	{
		thread.join();
	}
}