#pragma once
#include <mutex>
#include <iostream>
#include <future>

class Print
{
public:
	void PrintAsync(const char* thingToPrint);

private:
	std::mutex printMutex;
};

inline void Print::PrintAsync(const char* thingToPrint)
{
}