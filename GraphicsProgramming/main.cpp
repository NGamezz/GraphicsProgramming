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

struct Vertex
{
	unsigned int startIndex;
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;
};

typedef struct
{
	float x, y;
} Vector2;

int init(GLFWwindow*& window);
void CreateGeometry(GLuint& vao, GLuint& EBO, int& boxSize, int& boxIndexSize);
void ProcessInput(GLFWwindow*& window);

float static interpolate(const float a, const float b, const float value);
Vector2 static get_random_gradient(const int ix, const int iy);
float static dot_grid_gradient(const int ix, const int iy, const float x, const float y);
float perlin_noise(const float x, const float z);

void ProcessUniforms(unsigned int& program, WorldInformation& worldInformation, glm::mat4& worldMatrix);
void SetupInitialWorldInformation(WorldInformation& worldInformation);

float perlin_noise(float x, float z);

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
GLuint heightMapNormalID;

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
	heightMapNormalID = FileLoader::LoadGLTexture("Textures/HeightmapNormal.png");
	//GameManager gameManager;

	SetupInitialWorldInformation(worldInformation);

	//Create viewport
	glViewport(0, 0, width, height);

	double deltaTime = 0.0;
	double prevousTick = 0.0;
	double frameRate = 0.0;
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
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, heightMapNormalID);

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
	cameraPitch = std::min(89.9f, std::max(-89.9f, cameraPitch + dy));

	if (cameraYaw > 180.0f)
	{
		cameraYaw -= 360.0f;
	}
	if (cameraYaw < -180.0f)
	{
		cameraYaw += 360.0f;
	}

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

static glm::vec3 create_normal(const glm::vec3 a, const glm::vec3 b, const glm::vec3 c)
{
	auto cross = glm::cross(b - a, c - a);
	return glm::normalize(cross);
}

static float get_octaved_noise(const float x, const float y, const int octaves, const int size)
{
	float perlinValue = 0.0f;
	float frequency = 1.0f;
	float amplitude = 1.0f;

	for (int octave = 0; octave < 12; ++octave)
	{
		perlinValue += perlin_noise(x * frequency / size, y * frequency / size) * amplitude;

		amplitude *= 0.5f;
		frequency *= 2;
	}

	return perlinValue;
}

unsigned int GeneratePlane(const char* heightmap, GLenum format, int comp, float hScale, float xzScale, unsigned int& indexCount, unsigned int& heightmapID)
{
	int width = 0, height = 0, channels = 0;
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

	const int stride = 8;
	int count = width * height;

	const int gridSize = 400;

	float* vertices = new float[count * stride];
	unsigned int* indices = new unsigned int[(width - 1) * (height - 1) * 6];

	//Calculate Batch Size
	int threadCount = std::thread::hardware_concurrency();
	int batchSize = count / threadCount;
	int batches = count / batchSize;

	std::vector<std::thread> threads;

	std::vector<Vertex> vertexVector(count);

	//Process Primary Batches.
	for (int i = 0; i < batches; i++)
	{
		int start = i * batchSize;

		threads.emplace_back([=, &vertices]()
			{
				int vertexIndex = start * 8;
				int size = gridSize;

				for (int loopIndex = start; loopIndex < start + batchSize; ++loopIndex)
				{
					int x = loopIndex % width;
					int z = loopIndex / width;

					vertices[vertexIndex++] = x * xzScale;

					float perlinValue = get_octaved_noise(x, z, 12, size);

					vertices[vertexIndex++] = perlinValue * 500.0f;
					vertices[vertexIndex++] = z * xzScale;

					//Normals Need to be calculated.
					vertices[vertexIndex++] = 0;
					vertices[vertexIndex++] = 1;
					vertices[vertexIndex++] = 0;

					//Uvs
					vertices[vertexIndex++] = x / (float)width;
					vertices[vertexIndex++] = z / (float)height;
				}
			});
	}

	//Process Remainder
	int remaining = count % threadCount;

	if (remaining > 0)
	{
		int start = count - remaining;
		int vertexIndex = start * 8;

		for (int i = start; i < start + remaining; ++i)
		{
			int x = i % width;
			int z = i / width;

			vertices[vertexIndex++] = x * xzScale;

			float frequency = 1.0f;
			float amplitude = 1.0f;

			float perlinValue = get_octaved_noise(x, z, 12, gridSize);

			vertices[vertexIndex++] = perlinValue * 500.0f;
			vertices[vertexIndex++] = z * xzScale;

			glm::vec3 cross = glm::cross(glm::vec3(1), glm::vec3(1));

			//Normals
			vertices[vertexIndex++] = 0.0f;
			vertices[vertexIndex++] = 0.0f;
			vertices[vertexIndex++] = 0.0f;

			//Uvs
			vertices[vertexIndex++] = x / (float)width;
			vertices[vertexIndex++] = z / (float)height;
		}
	}

	for (auto& thread : threads)
	{
		thread.join();
	}
	threads.clear();

	int increment = stride * 3;
	for (unsigned int i{ 0 }; i < count * 8; i += increment)
	{
		std::vector<glm::vec3> verts(3);

		for (int vertex{ 0 }; vertex < 3; ++vertex)
		{
			auto index = i + (stride * vertex);

			auto x = vertices[index];
			auto y = vertices[index + 1];
			auto z = vertices[index + 2];

			verts[vertex] = glm::vec3(x, y, z);
		}

		auto normal = create_normal(verts[0], verts[1], verts[2]);

		for (int vertex{ 0 }; vertex < 3; ++vertex)
		{
			auto index = (i + 3) + (stride * vertex);

			vertices[index] = normal.x;
			vertices[index + 1] = normal.y;
			vertices[index + 2] = normal.z;
		}
	}

	unsigned int index = 0;
	for (int i{ 0 }; i < (width - 1) * (height - 1); ++i)
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

	indexCount = ((width - 1) * (height - 1) * 6);
	unsigned long vertSize = (static_cast<unsigned long long>(width) * height) * stride * sizeof(float);

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

float perlin_noise(const float x, const float z)
{
	int xGrid = (int)x;
	int yGrid = (int)z;
	int xGridB = xGrid + 1;
	int yGridB = yGrid + 1;

	float sx = x - (float)xGrid;
	float sy = z - (float)yGrid;

	float n0 = dot_grid_gradient(xGrid, yGrid, x, z);
	float n1 = dot_grid_gradient(xGridB, yGrid, x, z);
	float ix0 = interpolate(n0, n1, sx);

	n0 = dot_grid_gradient(xGrid, yGridB, x, z);
	n1 = dot_grid_gradient(xGridB, yGridB, x, z);
	float ix1 = interpolate(n0, n1, sx);

	float value = interpolate(ix0, ix1, sy);

	return value;
}

Vector2 static get_random_gradient(const int ix, const int iy)
{
	// No precomputed gradients mean this works for any number of grid coordinates
	const unsigned int w = 8 * sizeof(unsigned);
	const unsigned int s = w / 2;
	unsigned int a = ix, b = iy;
	a *= 3284157443;

	b ^= a << s | a >> w - s;
	b *= 1911520717;

	a ^= b << s | b >> w - s;
	a *= 2048419325;
	float random = a * (3.14159265 / ~(~0u >> 1)); // in [0, 2*Pi]

	// Create the vector from the angle
	Vector2 v{};
	v.x = sin(random);
	v.y = cos(random);

	return v;
}

float static dot_grid_gradient(const int ix, const int iy, const float x, const float y)
{
	Vector2 gradient = get_random_gradient(ix, iy);

	float dx = x - (float)ix;
	float dy = y - (float)iy;

	return (dx * gradient.x + dy * gradient.y);
}

float static interpolate(const float a, const float b, const float value)
{
	return (b - a) * (3.0 - value * 2.0) * value * value + a;
}