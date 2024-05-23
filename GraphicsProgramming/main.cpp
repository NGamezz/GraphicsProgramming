#include <GLAD/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION

#include "GameManager.h"
#include "Renderer.h"

#include "Parallel.h"
#include "ThreadPool.h"

#include "FileLoader.h"
#include "ActionQueue.h"

struct GeometryInformation
{
	glm::vec3 position;
	float vertices[1];
	float stride;
	int indices[];
};

struct WorldInformation
{
	glm::vec3 cameraPosition;
	glm::mat4 projection;
	glm::mat4 view;
	glm::vec3 cameraPos;
	std::vector<GeometryInformation> objectsToRender;
};

int init(GLFWwindow*& window);
void CreateGeometry(GLuint& vao, GLuint& EBO, int& triangleSize, int& triangleIndexSize);
void ProcessInput(GLFWwindow*& window);

void ProcessUniforms(unsigned int program, WorldInformation& worldInformation);
void SetupInitialWorldInformation(WorldInformation& worldInformation);

const int width = 1280, height = 720;

ActionQueue actionQueue;

std::jthread inputThread;
std::atomic_bool active;

int main()
{
	GLFWwindow* window = nullptr;
	auto result = init(window);

	if (result != 0)
		return result;

	active.store(true);
	inputThread = std::jthread([&window]()
		{
			ProcessInput(window);
		});
	inputThread.detach();

	GLuint vao, ebo;
	int triangleSize, triangleIndexSize;
	CreateGeometry(vao, ebo, triangleSize, triangleIndexSize);

	auto diffuse = FileLoader::LoadGLTexture("Textures/container2.png");
	auto normal = FileLoader::LoadGLTexture("Textures/container2_normal.png");

	std::unique_ptr<GameManager> gameManager = std::make_unique<GameManager>(GameManager());

	WorldInformation worldInformation;
	SetupInitialWorldInformation(worldInformation);

	GLuint simpleProgram;

	std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
	renderer->Intialize(simpleProgram);

	//Create viewport
	glViewport(0, 0, width, height);

	double deltaTime = 0.0;
	double prevousTick = 0.0;
	double frameRate = 0.0;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, diffuse);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, normal);

	//Game render loop
	while (!glfwWindowShouldClose(window))
	{
		//Calculate DeltaTime and FrameRate.
		auto time = glfwGetTime();
		deltaTime = time - prevousTick;
		prevousTick = time;
		frameRate = 1.0 / deltaTime;

		//background color set & render
		glClearColor(0, 0, 0, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		//input
		//ProcessInput(window);
		gameManager->OnUpdate(deltaTime, window);

		ProcessUniforms(simpleProgram, worldInformation);

		renderer->RenderLoop(window);

		//rendering
		glUseProgram(simpleProgram);

		glBindVertexArray(vao);
		glDrawElements(GL_TRIANGLES, triangleIndexSize, GL_UNSIGNED_INT, 0);

		glfwSwapBuffers(window);

		//events pollen
		glfwPollEvents();

		//Clear queued functions
		if (!actionQueue.IsEmpty())
			actionQueue.ClearFunctionQueue();
	}

	//Terminate
	active.store(false);
	inputThread.request_stop();
	glfwTerminate();
	return 0;
}

void SetupInitialWorldInformation(WorldInformation& worldInformation)
{
	worldInformation = WorldInformation();
	worldInformation.cameraPos = glm::vec3(0.0f, 2.5f, -5.0f);
	worldInformation.cameraPosition = worldInformation.cameraPos;
	//Alleen wanneer de camera zoomed ect. 0.1 ~= 10cm.
	worldInformation.projection = glm::perspective(glm::radians(45.0f), width / (float)height, 0.1f, 100.0f);
	//1 Camera Position, camera directie, omhoog directie
	worldInformation.view = glm::lookAt(worldInformation.cameraPos, glm::vec3(0, 0, 0), glm::vec3(0.0f, 1.0f, 0.0f));
}

void ProcessUniforms(unsigned int program, WorldInformation& worldInformation)
{
	//Matrices
	glm::mat4 world = glm::mat4(1.0f);
	world = glm::rotate(world, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	world = glm::scale(world, glm::vec3(1.0f, 1.0f, 1.0f));
	world = glm::translate(world, glm::vec3(0.0f, 0.0f, 0.0f));

	glm::vec3 lightPositions = glm::vec3(3, 3, 1);

	//V = value pointer, sturen referentie naar instance door.
	glUniformMatrix4fv(glGetUniformLocation(program, "world"), 1, GL_FALSE, glm::value_ptr(world));
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(worldInformation.projection));
	glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(worldInformation.view));

	glUniform3fv(glGetUniformLocation(program, "lightPosition"), 1, glm::value_ptr(lightPositions));
	glUniform3fv(glGetUniformLocation(program, "cameraPosition"), 1, glm::value_ptr(worldInformation.cameraPos));
}

void ProcessInput(GLFWwindow*& window)
{
	while (active.load())
	{
		if (glfwGetKey(window, GLFW_KEY_ESCAPE))
		{
			glfwSetWindowShouldClose(window, true);
		}
	}
}

int init(GLFWwindow*& window)
{
	if (!glfwInit())
	{
		std::cout << "Failed to Initialize GLFW." << std::endl;
		return -1;
	}

	//Tells Glfw which profile & openGL version we're using.
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	int windowWidth = width;
	int windowHeight = height;

	//Make window
	window = glfwCreateWindow(windowWidth, windowHeight, "GLFWindow", nullptr, nullptr);

	if (window == nullptr)
	{
		std::cout << "Failed to create window" << std::endl;
		return -1;
	}

	//Set Context
	glfwMakeContextCurrent(window);

	//GLAD spul
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to load GLAD" << std::endl;
		glfwTerminate();
		return -2;
	}

	return 0;
}

void CreateGeometry(GLuint& VAO, GLuint& EBO, int& size, int& numIndices)
{
	// need 24 vertices for normal/uv-mapped Cube
	float vertices[] = {
		// positions            //colors            // tex coords   // normals          //tangents      //bitangents
		0.5f, -0.5f, -0.5f,     1.0f, 1.0f, 1.0f,   1.f, 1.f,       0.f, -1.f, 0.f,     -1.f, 0.f, 0.f,  0.f, 0.f, 1.f,
		0.5f, -0.5f, 0.5f,      1.0f, 1.0f, 1.0f,   1.f, 0.f,       0.f, -1.f, 0.f,     -1.f, 0.f, 0.f,  0.f, 0.f, 1.f,
		-0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 1.0f,   0.f, 0.f,       0.f, -1.f, 0.f,     -1.f, 0.f, 0.f,  0.f, 0.f, 1.f,
		-0.5f, -0.5f, -.5f,     1.0f, 1.0f, 1.0f,   0.f, 1.f,       0.f, -1.f, 0.f,     -1.f, 0.f, 0.f,  0.f, 0.f, 1.f,

		0.5f, 0.5f, -0.5f,      1.0f, 1.0f, 1.0f,   1.f, 1.f,       1.f, 0.f, 0.f,     0.f, -1.f, 0.f,  0.f, 0.f, 1.f,
		0.5f, 0.5f, 0.5f,       1.0f, 1.0f, 1.0f,   1.f, 0.f,       1.f, 0.f, 0.f,     0.f, -1.f, 0.f,  0.f, 0.f, 1.f,

		0.5f, 0.5f, 0.5f,       1.0f, 1.0f, 1.0f,   1.f, 0.f,       0.f, 0.f, 1.f,     1.f, 0.f, 0.f,  0.f, -1.f, 0.f,
		-0.5f, 0.5f, 0.5f,      1.0f, 1.0f, 1.0f,   0.f, 0.f,       0.f, 0.f, 1.f,     1.f, 0.f, 0.f,  0.f, -1.f, 0.f,

		-0.5f, 0.5f, 0.5f,      1.0f, 1.0f, 1.0f,   0.f, 0.f,      -1.f, 0.f, 0.f,     0.f, 1.f, 0.f,  0.f, 0.f, 1.f,
		-0.5f, 0.5f, -.5f,      1.0f, 1.0f, 1.0f,   0.f, 1.f,      -1.f, 0.f, 0.f,     0.f, 1.f, 0.f,  0.f, 0.f, 1.f,

		-0.5f, 0.5f, -.5f,      1.0f, 1.0f, 1.0f,   0.f, 1.f,      0.f, 0.f, -1.f,     1.f, 0.f, 0.f,  0.f, 1.f, 0.f,
		0.5f, 0.5f, -0.5f,      1.0f, 1.0f, 1.0f,   1.f, 1.f,      0.f, 0.f, -1.f,     1.f, 0.f, 0.f,  0.f, 1.f, 0.f,

		-0.5f, 0.5f, -.5f,      1.0f, 1.0f, 1.0f,   1.f, 1.f,       0.f, 1.f, 0.f,     1.f, 0.f, 0.f,  0.f, 0.f, 1.f,
		-0.5f, 0.5f, 0.5f,      1.0f, 1.0f, 1.0f,   1.f, 0.f,       0.f, 1.f, 0.f,     1.f, 0.f, 0.f,  0.f, 0.f, 1.f,

		0.5f, -0.5f, 0.5f,      1.0f, 1.0f, 1.0f,   1.f, 1.f,       0.f, 0.f, 1.f,     1.f, 0.f, 0.f,  0.f, -1.f, 0.f,
		-0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 1.0f,   0.f, 1.f,       0.f, 0.f, 1.f,     1.f, 0.f, 0.f,  0.f, -1.f, 0.f,

		-0.5f, -0.5f, 0.5f,     1.0f, 1.0f, 1.0f,   1.f, 0.f,       -1.f, 0.f, 0.f,     0.f, 1.f, 0.f,  0.f, 0.f, 1.f,
		-0.5f, -0.5f, -.5f,     1.0f, 1.0f, 1.0f,   1.f, 1.f,       -1.f, 0.f, 0.f,     0.f, 1.f, 0.f,  0.f, 0.f, 1.f,

		-0.5f, -0.5f, -.5f,     1.0f, 1.0f, 1.0f,   0.f, 0.f,       0.f, 0.f, -1.f,     1.f, 0.f, 0.f,  0.f, 1.f, 0.f,
		0.5f, -0.5f, -0.5f,     1.0f, 1.0f, 1.0f,   1.f, 0.f,       0.f, 0.f, -1.f,     1.f, 0.f, 0.f,  0.f, 1.f, 0.f,

		0.5f, -0.5f, -0.5f,     1.0f, 1.0f, 1.0f,   0.f, 1.f,       1.f, 0.f, 0.f,     0.f, -1.f, 0.f,  0.f, 0.f, 1.f,
		0.5f, -0.5f, 0.5f,      1.0f, 1.0f, 1.0f,   0.f, 0.f,       1.f, 0.f, 0.f,     0.f, -1.f, 0.f,  0.f, 0.f, 1.f,

		0.5f, 0.5f, -0.5f,      1.0f, 1.0f, 1.0f,   0.f, 1.f,       0.f, 1.f, 0.f,     1.f, 0.f, 0.f,  0.f, 0.f, 1.f,
		0.5f, 0.5f, 0.5f,       1.0f, 1.0f, 1.0f,   0.f, 0.f,       0.f, 1.f, 0.f,     1.f, 0.f, 0.f,  0.f, 0.f, 1.f
	};

	unsigned int indices[] = {  // note that we start from 0!
		// DOWN
		0, 1, 2,   // first triangle
		0, 2, 3,    // second triangle
		// BACK
		14, 6, 7,   // first triangle
		14, 7, 15,    // second triangle
		// RIGHT
		20, 4, 5,   // first triangle
		20, 5, 21,    // second triangle
		// LEFT
		16, 8, 9,   // first triangle
		16, 9, 17,    // second triangle
		// FRONT
		18, 10, 11,   // first triangle
		18, 11, 19,    // second triangle
		// UP
		22, 12, 13,   // first triangle
		22, 13, 23,    // second triangle
	};

	int stride = (3 + 3 + 2 + 3 + 3 + 3) * sizeof(float);

	size = sizeof(vertices) / stride;
	numIndices = sizeof(indices) / sizeof(int);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	//If pointer needs to be 8 do (const void*)8)
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(0);

	//If pointer needs to be 8 do (const void*)8)
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(3, 3, GL_FLOAT, GL_TRUE, stride, (void*)(8 * sizeof(float)));
	glEnableVertexAttribArray(3);

	glVertexAttribPointer(4, 3, GL_FLOAT, GL_TRUE, stride, (void*)(11 * sizeof(float)));
	glEnableVertexAttribArray(4);

	glVertexAttribPointer(5, 3, GL_FLOAT, GL_TRUE, stride, (void*)(14 * sizeof(float)));
	glEnableVertexAttribArray(5);
}