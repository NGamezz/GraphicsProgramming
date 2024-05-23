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

#include "Print.h"

#include "FileLoader.h"
#include "ActionQueue.h"

#include <glm/gtc/quaternion.hpp>

struct GeometryInformation
{
	glm::vec3 position;
	float vertices[1];
	float stride;
	int indices[];
};

struct WorldInformation
{
	glm::vec3 cameraPosition = glm::vec3();
	glm::mat4 projection = glm::mat4();
	glm::mat4 view = glm::mat4();
	glm::vec3 lightPosition = glm::vec3();
	std::vector<GeometryInformation> objectsToRender;
};

int init(GLFWwindow*& window);
void CreateGeometry(GLuint& vao, GLuint& EBO, int& boxSize, int& boxIndexSize);
void ProcessInput(GLFWwindow*& window);

void ProcessUniforms(unsigned int& program, WorldInformation& worldInformation, glm::mat4& worldMatrix);
void SetupInitialWorldInformation(WorldInformation& worldInformation);

void RenderPlane(unsigned int& planeProgram);
unsigned int GeneratePlane(const char* heightmap, GLenum format, int comp, float hScale, float xzScale, unsigned int& indexCount, unsigned int& heightmapID);

void RenderSkyBox(unsigned int& skyProgram);

void MousePosCallBack(GLFWwindow* window, double xPos, double yPos);

void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);

bool keys[1024];

const int width = 1280, height = 720;

ActionQueue actionQueue;

WorldInformation worldInformation;

GLuint boxVao, boxEbo;
int boxSize, boxIndexSize;

float cameraYaw, cameraPitch;
float lastX, lastY;
bool firstMouse = true;

GLuint simpleProgram, skyBoxProgram, terrainProgram;

glm::quat camQuaternion = glm::quat(glm::vec3(glm::radians(cameraPitch), glm::radians(cameraYaw), 0.0f));

//terrain data
GLuint terrainVAO, heightMapId, terrainIndexCount;

int main()
{
	GLFWwindow* window = nullptr;
	auto result = init(window);

	if (result != 0)
		return result;

	int concurrency = std::thread::hardware_concurrency();
	ThreadPool threadPool(concurrency);

	CreateGeometry(boxVao, boxEbo, boxSize, boxIndexSize);

	std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
	renderer->Intialize(simpleProgram);
	renderer->createProgram(skyBoxProgram, "Shaders/skyVertexShader.glsl", "Shaders/skyFragmentShader.glsl");
	renderer->createProgram(terrainProgram, "Shaders/simpleTerrainVertex.glsl", "Shaders/simpleTerrainFragment.glsl");

	terrainVAO = GeneratePlane("Textures/Heightmap2.png", GL_RGBA, 4, 100.0f, 5.0f, terrainIndexCount, heightMapId);

	auto diffuse = FileLoader::LoadGLTexture("Textures/container2.png");
	auto normal = FileLoader::LoadGLTexture("Textures/container2_normal.png");

	std::unique_ptr<GameManager> gameManager = std::make_unique<GameManager>(GameManager());

	SetupInitialWorldInformation(worldInformation);

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
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//input
		gameManager->OnUpdate(deltaTime, window);
		ProcessInput(window);

		////rendering
		RenderSkyBox(skyBoxProgram);
		RenderPlane(terrainProgram);
		renderer->RenderLoop(window);

		glfwSwapBuffers(window);
		glfwPollEvents();

		//Clear queued functions
		if (!actionQueue.IsEmpty())
			actionQueue.ClearFunctionQueue();
	}

	//Terminate
	threadPool.~ThreadPool();
	glfwTerminate();
	return 0;
}

void KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		keys[key] = true;
	}
	else if (action == GLFW_RELEASE)
	{
		keys[key] = false;
	}
}

void RenderPlane(unsigned int& planeProgram)
{
	glEnable(GL_DEPTH);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glUseProgram(planeProgram);

	glm::mat4 world = glm::mat4(1.0f);

	ProcessUniforms(planeProgram, worldInformation, world);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, heightMapId);

	glBindVertexArray(terrainVAO);
	glDrawElements(GL_TRIANGLES, terrainIndexCount, GL_UNSIGNED_INT, 0);
	glUseProgram(0);
}

void SetupInitialWorldInformation(WorldInformation& worldInformation)
{
	worldInformation = WorldInformation();
	worldInformation.lightPosition = glm::normalize(glm::vec3(-0.5, -0.5f, -0.5f));
	worldInformation.cameraPosition = glm::vec3(100.0f, 100.0f, 100.0f);
	//Alleen wanneer de camera zoomed ect. 0.1 ~= 10cm.
	worldInformation.projection = glm::perspective(glm::radians(60.0f), width / (float)height, 0.1f, 5000.0f);
	//1 Camera Position, camera directie, omhoog directie
	worldInformation.view = glm::lookAt(worldInformation.cameraPosition, glm::vec3(0, 0, 0), glm::vec3(0.0f, 1.0f, 0.0f));
}

void MousePosCallBack(GLFWwindow* window, double xPos, double yPos)
{
	//Op basis van pitch en yaw roteren we een genormalizeerde vector3.
	float x = (float)xPos;
	float y = (float)yPos;

	if (firstMouse)
	{
		lastX = x;
		lastY = y;
		firstMouse = false;
	}

	float dx = x - lastX;
	float dy = y - lastY;

	lastX = x;
	lastY = y;

	//x yaw, y pitch

	cameraYaw -= dx;
	cameraPitch = glm::clamp(cameraPitch + dy, -89.9f, 89.9f);

	if (cameraYaw > 180.0f)
		cameraYaw *= -360.0f;
	if (cameraYaw < -180.0f)
		cameraYaw += 360.0f;

	camQuaternion = glm::quat(glm::vec3(glm::radians(cameraPitch), glm::radians(cameraYaw), 0.0f));

	//Quaternion x Vector.
	glm::vec3 camForward = camQuaternion * glm::vec3(0, 0, 1);
	glm::vec3 camUp = camQuaternion * glm::vec3(0, 1, 0);

	worldInformation.view = glm::lookAt(worldInformation.cameraPosition, worldInformation.cameraPosition + camForward, camUp);
}

void RenderSkyBox(unsigned int& skyProgram)
{
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(skyProgram);

	glm::mat4 world = glm::mat4(1.0f);
	world = glm::translate(world, worldInformation.cameraPosition);
	world = glm::scale(world, glm::vec3(100.0f, 100.0f, 100.0f));

	ProcessUniforms(skyProgram, worldInformation, world);

	glBindVertexArray(boxVao);
	glDrawElements(GL_TRIANGLES, boxIndexSize, GL_UNSIGNED_INT, 0);

	glEnable(GL_DEPTH);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glUseProgram(0);
}

void ProcessUniforms(unsigned int& program, WorldInformation& worldInformation, glm::mat4& worldMatrix)
{
	glUniformMatrix4fv(glGetUniformLocation(program, "world"), 1, GL_FALSE, glm::value_ptr(worldMatrix));
	//V = value pointer, sturen referentie naar instance door.
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(worldInformation.projection));
	glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(worldInformation.view));

	glUniform3fv(glGetUniformLocation(program, "lightDirection"), 1, glm::value_ptr(worldInformation.lightPosition));
	glUniform3fv(glGetUniformLocation(program, "cameraPosition"), 1, glm::value_ptr(worldInformation.cameraPosition));
}

void ProcessInput(GLFWwindow*& window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE))
	{
		glfwSetWindowShouldClose(window, true);
	}

	bool camChanged = false;
	if (keys[GLFW_KEY_W])
	{
		worldInformation.cameraPosition += camQuaternion * glm::vec3(0, 0, 1);
		camChanged = true;
	}
	if (keys[GLFW_KEY_S])
	{
		worldInformation.cameraPosition += camQuaternion * glm::vec3(0, 0, -1);
		camChanged = true;
	}
	if (keys[GLFW_KEY_A])
	{
		worldInformation.cameraPosition += camQuaternion * glm::vec3(1, 0, 0);
		camChanged = true;
	}
	if (keys[GLFW_KEY_D])
	{
		worldInformation.cameraPosition += camQuaternion * glm::vec3(-1, 0, 0);
		camChanged = true;
	}

	if (camChanged)
	{
		glm::vec3 camForward = camQuaternion * glm::vec3(0, 0, 1);
		glm::vec3 camUp = camQuaternion * glm::vec3(0, 1, 0);
		worldInformation.view = glm::lookAt(worldInformation.cameraPosition, worldInformation.cameraPosition + camForward, camUp);
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

	//Register CallBacks
	glfwSetCursorPosCallback(window, MousePosCallBack);
	glfwSetKeyCallback(window, KeyCallBack);

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

unsigned int GeneratePlane(const char* heightmap, GLenum format, int comp, float hScale, float xzScale, unsigned int& indexCount, unsigned int& heightmapID)
{
	int width, height, channels;
	unsigned char* data = nullptr;
	if (heightmap != nullptr)
	{
		data = stbi_load(heightmap, &width, &height, &channels, comp);
		if (data)
		{
			glGenTextures(1, &heightmapID);
			glBindTexture(GL_TEXTURE_2D, heightmapID);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

			glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
			glGenerateMipmap(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	int stride = 8;
	float* vertices = new float[(width * height) * stride];
	unsigned int* indices = new unsigned int[(width - 1) * (height - 1) * 6];

	int index = 0;
	for (int i = 0; i < (width * height); i++)
	{
		// TODO: calculate x/z values
		int x = i % width;
		int z = i / width;

		// TODO: set position
		vertices[index++] = x * xzScale;
		//y
		vertices[index++] = 0;
		vertices[index++] = z * xzScale;

		// TODO: set normal
		vertices[index++] = 0;
		vertices[index++] = 1;
		vertices[index++] = 0;

		// TODO: set uv
		vertices[index++] = x / (float)width;
		vertices[index++] = z / (float)height;
	}

	// OPTIONAL TODO: Calculate normal
	// TODO: Set normal

	index = 0;
	for (int i = 0; i < (width - 1) * (height - 1); i++)
	{
		int x = i % (width - 1);
		int z = i / (width - 1);

		int vertex = z * width + x;

		indices[index++] = vertex;
		indices[index++] = vertex + width;
		indices[index++] = vertex + width + 1;
		indices[index++] = vertex;
		indices[index++] = vertex + width + 1;
		indices[index++] = vertex + 1;
	}

	unsigned int vertSize = (width * height) * stride * sizeof(float);
	indexCount = ((width - 1) * (height - 1) * 6);

	unsigned int VAO, VBO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertSize, vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_STATIC_DRAW);

	// vertex information!
	// position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * stride, 0);
	glEnableVertexAttribArray(0);
	// normal
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * stride, (void*)(sizeof(float) * 3));
	glEnableVertexAttribArray(1);
	// uv
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * stride, (void*)(sizeof(float) * 6));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	delete[] vertices;
	delete[] indices;

	stbi_image_free(data);

	return VAO;
}