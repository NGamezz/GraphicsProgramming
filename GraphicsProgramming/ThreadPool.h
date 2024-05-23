#pragma once
#include <vector>
#include <mutex>
#include <queue>
#include <functional>

class ThreadPool
{
public:

	ThreadPool(unsigned int numThreads) : stop(false), threads(numThreads)
	{
		if (numThreads < 1)
			return;

		for (unsigned int i = 0; i < numThreads; ++i)
		{
			threads.emplace_back([&]
				{
					while (!stop)
					{
						std::function<void()> task;
						{
							std::unique_lock<std::mutex> lock(this->queue_mutex);

							this->condition.wait(lock, [&] { return this->stop || !this->tasks.empty(); });
							
							if (this->stop && this->tasks.empty()) {
								break;
							}
							
							task = std::move(this->tasks.front());
							this->tasks.pop();
						}
						task();
					}
				});
		}
	}

	template<typename F>
	void enqueue(F&& task);

	~ThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			stop.store(true, std::memory_order_relaxed);
		}
		condition.notify_all();
		
		for (std::thread& thread : threads)
		{
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