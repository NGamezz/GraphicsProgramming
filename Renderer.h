#pragma once

#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include "FileLoader.h"
#include <iostream>

//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

class Renderer
{
public:
	void RenderLoop(GLFWwindow*& window);
	void Intialize(GLuint& program);
	void createProgram(GLuint& programId, const char* vertex, const char* fragment);
private:
};