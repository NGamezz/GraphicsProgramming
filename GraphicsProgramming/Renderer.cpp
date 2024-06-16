#include "Renderer.h"

void Renderer::RenderLoop(GLFWwindow*& window)
{

}

void Renderer::Intialize(GLuint& program)
{
	createProgram(program, "Resources/Shaders/simpleVertexShader.glsl", "Resources/Shaders/simpleFragmentShader.glsl");

	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "mainTex"), 0);
	glUniform1i(glGetUniformLocation(program, "normalTex"), 1);
}

void create_programs()
{

}

void Renderer::createProgram(GLuint& programId, const char* vertex, const char* fragment)
{
	auto vertexFuture = FileLoader::LoadFileAsync(vertex);
	auto fragmentFuture = FileLoader::LoadFileAsync(fragment);

	GLuint vertexShaderId, fragmentShaderId;

	char infoLog[512];
	int succes;

	char* vertexSrc = vertexFuture.get();

	if (vertexSrc == nullptr)
	{
		std::cout << "Failed to Load Vertex" << std::endl;
	}

	vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShaderId, 1, &vertexSrc, nullptr);
	glCompileShader(vertexShaderId);

	glGetShaderiv(vertexShaderId, GL_COMPILE_STATUS, &succes);
	if (!succes)
	{
		glGetShaderInfoLog(vertexShaderId, 512, nullptr, infoLog);
		std::cout << "Compile Error, Vertex Shader\n" << infoLog << std::endl;
	}

	char* fragmentSrc = fragmentFuture.get();

	if (fragmentSrc == nullptr)
	{
		std::cout << "Failed to Load Fragment" << std::endl;
	}

	fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShaderId, 1, &fragmentSrc, nullptr);
	glCompileShader(fragmentShaderId);

	glGetShaderiv(fragmentShaderId, GL_COMPILE_STATUS, &succes);
	if (!succes)
	{
		glGetShaderInfoLog(vertexShaderId, 512, nullptr, infoLog);
		std::cout << "Compile Error, Fragment Shader\n" << infoLog << std::endl;
	}

	programId = glCreateProgram();
	glAttachShader(programId, vertexShaderId);
	glAttachShader(programId, fragmentShaderId);
	glLinkProgram(programId);

	glGetProgramiv(programId, GL_LINK_STATUS, &succes);
	if (!succes)
	{
		glGetProgramInfoLog(programId, 512, nullptr, infoLog);
		std::cout << "Program Linking Error" << infoLog << std::endl;
	}

	glDeleteShader(vertexShaderId);
	glDeleteShader(fragmentShaderId);

	delete(vertexSrc);
	delete(fragmentSrc);
}