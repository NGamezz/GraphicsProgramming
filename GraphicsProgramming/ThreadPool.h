#pragma once
#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <iostream>

class ThreadPool
{
public:

	ThreadPool(size_t numThreads) : stop(false), threads(numThreads)
	{
		if (numThreads < 1)
			throw std::invalid_argument("Number of threads must be at least 1");

		threads.reserve(numThreads);

		for (unsigned int i = 0; i < numThreads; ++i)
		{
			threads.emplace_back([this]
				{
					while (true)
					{
						std::function<void()> task;

						{
							std::unique_lock<std::mutex> lock(queue_mutex);
							condition.wait(lock, [this] { return stop.load() || !tasks.empty(); });

							if (stop.load() && tasks.empty())
								return;

							task = std::move(tasks.front());
							tasks.pop();
						}
						task();
					}
				});
		}
	}

	template<typename F>
	void enqueue(F&& task);

	~ThreadPool()
	{
		stop.store(true);
		condition.notify_all();

		for (std::thread& thread : threads)
		{
			if (thread.joinable())
				thread.join();
		}
	}

private:
	std::vector<std::thread> threads;
	std::mutex queue_mutex;
	std::queue<std::function<void()>> tasks;
	std::condition_variable condition;
	std::atomic_bool stop;
};

template<typename F>
inline void ThreadPool::enqueue(F&& task)
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		tasks.emplace(std::forward<F>(task));
	}
	condition.notify_one();
}