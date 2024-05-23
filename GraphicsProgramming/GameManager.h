#pragma once
#include <GLFW/glfw3.h>
#include <iostream>

class GameManager
{
public:
	void OnUpdate(double deltaTime, GLFWwindow* window);

private:
	void GatherInput();
	void ApplyInput(double deltaTime);
	float xInput = 0.0f;
	float yInput = 0.0f;
};