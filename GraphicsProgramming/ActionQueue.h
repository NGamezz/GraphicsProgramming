#pragma once

#include <mutex>
#include <queue>
#include <functional>

class ActionQueue
{
public:
	static ActionQueue& shared_instance() { static ActionQueue queue; return queue; }

	void AddActionToQueue(std::function<void()> func);
	void ClearFunctionQueue();
	bool IsEmpty();

private:
	std::mutex queueMutex;
	std::queue<std::function<void()>> functionQueue;
};

inline void ActionQueue::AddActionToQueue(std::function<void()> func)
{
	std::unique_lock<std::mutex> lock(queueMutex);
	functionQueue.emplace(func);
}

inline void ActionQueue::ClearFunctionQueue()
{
	std::unique_lock<std::mutex> lock(queueMutex);
	for (int i = functionQueue.size() - 1; i >= 0; i--)
	{
		auto& func = functionQueue.front();
		func();
		functionQueue.pop();
	}
}

inline bool ActionQueue::IsEmpty()
{
	return functionQueue.empty();
}