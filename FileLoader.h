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
	static unsigned int LoadGLTexture(const char* filePath, int comp = 0);
	static std::future<char*> LoadFileAsync(const char* filePath);
private:
};

