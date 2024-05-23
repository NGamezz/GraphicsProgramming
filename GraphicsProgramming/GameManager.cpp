#include "GameManager.h"

void GameManager::OnUpdate(double deltaTime, GLFWwindow* window)
{
	GatherInput();
	ApplyInput(deltaTime);
}

void GameManager::GatherInput()
{
}

void GameManager::ApplyInput(double deltaTime)
{
}