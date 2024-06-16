#pragma once

#include <functional>
#include <thread>
#include <GLAD/glad.h>
#include "stb_image.h"
#include <iostream>
#include <future>
#include <fstream>

static class FileLoader
{
public:
	static unsigned int load_GL_texture(const char* filePath, int comp = 0);
	static std::future<char*> load_file_async(const char* filePath);
};