#include "Parallel.h"

void Parallel::parallelFor(const int start, const int end, const std::function<void(int)> body, ThreadPool& threadPool)
{
	for (int i = start; i < end; ++i)
	{
		threadPool.enqueue([=]() {body(i); });
	}
	return;
}