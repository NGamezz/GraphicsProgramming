#pragma once

#define STB_IMAGE_IMPLEMENTATION

#include "Renderer.h"

#include <GLFW/glfw3.h>

#include "ThreadPool.h"
#include "perlin_noise.hpp"
#include "ActionQueue.h"

struct Entity
{
	Model* model;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
};

int initialize_window(GLFWwindow*& window);

void create_cube(GLuint& vao, GLuint& EBO, int& skyBoxSize, int& skyBoxIndexSize);
void process_input(GLFWwindow*& window);

void initialize_world_information(WorldInformation& worldInformation);

void generate_landscape_chunk(const glm::vec2 currentChunkCord, const glm::vec3 position, const int size, float hScale, float xzScale, glm::vec3 offset, int concurrencyLevel = -1);

void create_shaders(Renderer& renderer);

void mouse_call_back(GLFWwindow* window, double xPos, double yPos);

void key_call_back(GLFWwindow* window, int key, int scancode, int action, int mods);

void check_visible_planes();
void load_textures();

void load_models(std::vector<Entity>& entities);

bool keys[1024];

const int width = 1280, height = 720;

WorldInformation worldInformation;

GLuint skyBoxVao, skyBoxEbo;
int skyBoxSize, skyBoxIndexSize;

float cameraYaw, cameraPitch;
float lastX, lastY;
bool firstMouse = true;

struct vec2compare
{
	bool operator()(const glm::vec2& a, const glm::vec2& b) const
	{
		if (a.x != b.x)
			return a.x < b.x;
		return a.y < b.y;
	}
};

GLuint skyBoxProgram, cubeProgram, terrainProgram, modelProgram;

glm::quat camQuaternion = glm::quat(glm::vec3(glm::radians(cameraPitch), glm::radians(cameraYaw), 0.0f));

std::vector<Entity> entities;

std::map<glm::vec2, Plane, vec2compare> activeTerrainChunks;

const int chunkSize = 241;

const int xScale = 5;

int systemThreadsCount;
const int maxViewDistance = 400;

ThreadPool threadPool(std::thread::hardware_concurrency());

Renderer renderer;

Cube cube;

int main()
{
	systemThreadsCount = std::thread::hardware_concurrency();

	GLFWwindow* window = nullptr;
	auto result = initialize_window(window);

	if (result != 0) return result;

	create_cube(skyBoxVao, skyBoxEbo, skyBoxSize, skyBoxIndexSize);
	create_cube(cube.VAO, cube.EBO, cube.size, cube.IndexSize);

	create_shaders(renderer);

	int count = 0;

	load_models(entities);
	load_textures();

	initialize_world_information(worldInformation);

	//Create viewport
	glViewport(0, 0, width, height);

	double deltaTime = 0.0;
	double prevousTick = 0.0;
	double frameRate = 0.0;

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
		process_input(window);

		//Update Sun Color.
		worldInformation.lightPosition = glm::vec3(glm::cos(time * 0.3f), glm::sin(time * 0.3f), 0.0f);

		////rendering
		renderer.render_skybox(skyBoxProgram, worldInformation, skyBoxVao, skyBoxIndexSize);
		renderer.render_cube(cubeProgram, worldInformation, cube);

		for (auto& entity : entities)
		{
			renderer.render_model(entity.model, modelProgram, worldInformation, entity.position, entity.rotation, entity.scale);
		}

		for (auto& plane : activeTerrainChunks)
		{
			renderer.render_plane(terrainProgram, plane.second, worldInformation);
		}

		check_visible_planes();

		glfwSwapBuffers(window);
		glfwPollEvents();

		//Clear queued functions
		if (!ActionQueue::shared_instance().IsEmpty())
			ActionQueue::shared_instance().ClearFunctionQueue();
	}

	//Dispose of entity pointers.
	for (auto& entity : entities)
	{
		delete entity.model;
	}

	//Terminate
	threadPool.~ThreadPool();
	glfwTerminate();
	return 0;
}

void check_visible_planes()
{
	const int size = chunkSize - 1;
	auto visibleChunks = static_cast<int>(std::round(maxViewDistance / size));
	const float chunkOffset = size * xScale;

	auto cameraPos = worldInformation.cameraPosition;
	glm::vec2 currentCord = glm::vec2(floor(cameraPos.x / chunkSize), floor(cameraPos.z / chunkSize));

	for (int yOffset = -visibleChunks; yOffset <= visibleChunks; ++yOffset)
	{
		for (int xOffset = -visibleChunks; xOffset <= visibleChunks; ++xOffset)
		{
			glm::vec2 currentChunkCord = glm::vec2((xOffset + currentCord.x), (yOffset + currentCord.y));

			if (activeTerrainChunks.contains(currentChunkCord))
			{
				//Need to deactivate non active ones.
				continue;
			}
			else
			{
				glm::vec3 chunkWorldPos = glm::vec3(currentChunkCord.x * chunkOffset, 0.0f, currentChunkCord.y * chunkOffset);
				activeTerrainChunks.insert({ currentChunkCord, Plane() });

				//Queue it to the threadpool for execution.
				threadPool.enqueue([=]()
					{
						generate_landscape_chunk(currentChunkCord, chunkWorldPos, chunkSize, 400.0f, xScale, chunkWorldPos, 1);
					});
			}
		}
	}
}

void load_models(std::vector<Entity>& entities)
{
	Entity templeEntity{};

	templeEntity.model = new Model("Resources/Models/Japanese_Temple_Model/Japanese_Temple.obj");
	templeEntity.position = glm::vec3(500, 0, 500);
	templeEntity.rotation = glm::vec3(0, 0.1f, 0.0f);
	templeEntity.scale = glm::vec3(25);

	//Temple doesn't need the uvs to be flipped. Backpack Does.
	stbi_set_flip_vertically_on_load(true);

	Entity backPack{};

	backPack.model = new Model("Resources/Models/backpack/backpack.obj");
	backPack.position = glm::vec3(750, 150, 750);
	backPack.rotation = glm::vec3(0);
	backPack.scale = glm::vec3(50);

	std::cout << "Finished Loading Models" << std::endl;

	entities.push_back(backPack);
	entities.push_back(templeEntity);
}

void create_shaders(Renderer& renderer)
{
	renderer.Intialize(cubeProgram);
	renderer.createProgram(skyBoxProgram, "Resources/Shaders/skyVertexShader.glsl", "Resources/Shaders/skyFragmentShader.glsl");
	renderer.createProgram(terrainProgram, "Resources/Shaders/simpleTerrainVertex.glsl", "Resources/Shaders/simpleTerrainFragment.glsl");
	renderer.createProgram(modelProgram, "Resources/Shaders/modelVertex.glsl", "Resources/Shaders/modelFragment.glsl");
}

void key_call_back(GLFWwindow* window, int key, int scancode, int action, int mods)
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

void load_textures()
{
	//Textures for the terrain.
	renderer.dirt = FileLoader::load_GL_texture("Resources/Textures/dirt.jpg");
	renderer.sand = FileLoader::load_GL_texture("Resources/Textures/sand.jpg");
	renderer.grass = FileLoader::load_GL_texture("Resources/Textures/grass.png", 4);
	renderer.rock = FileLoader::load_GL_texture("Resources/Textures/rock.jpg");
	renderer.snow = FileLoader::load_GL_texture("Resources/Textures/snow.jpg");

	glUseProgram(terrainProgram);

	glUniform1i(glGetUniformLocation(terrainProgram, "dirt"), 0);
	glUniform1i(glGetUniformLocation(terrainProgram, "sand"), 1);
	glUniform1i(glGetUniformLocation(terrainProgram, "grass"), 2);
	glUniform1i(glGetUniformLocation(terrainProgram, "rock"), 3);
	glUniform1i(glGetUniformLocation(terrainProgram, "snow"), 4);

	//Texture setup for the models.
	glUseProgram(modelProgram);

	glUniform1i(glGetUniformLocation(modelProgram, "texture_diffuse1"), 0);
	glUniform1i(glGetUniformLocation(modelProgram, "texture_specular1"), 1);
	glUniform1i(glGetUniformLocation(modelProgram, "texture_normal1"), 2);
	glUniform1i(glGetUniformLocation(modelProgram, "texture_roughness1"), 3);
	glUniform1i(glGetUniformLocation(modelProgram, "texture_ao1"), 4);

	//Textures for the Box.
	auto cubeDiffuse = FileLoader::load_GL_texture("Resources/Textures/container2.png");
	auto cubeNormal = FileLoader::load_GL_texture("Resources/Textures/container2_normal.png");

	cube.Textures.push_back(cubeDiffuse);
	cube.Textures.push_back(cubeNormal);
}

void initialize_world_information(WorldInformation& worldInformation)
{
	worldInformation = WorldInformation();
	worldInformation.lightPosition = glm::normalize(glm::vec3(-0.5, -0.5f, -0.5f));
	worldInformation.cameraPosition = glm::vec3(100.0f, 100.0f, 100.0f);
	//Alleen wanneer de camera zoomed ect. 0.1 ~= 10cm.
	worldInformation.projection = glm::perspective(glm::radians(60.0f), width / (float)height, 0.1f, 5000.0f);
	//1 Camera Position, camera directie, omhoog directie
	worldInformation.view = glm::lookAt(worldInformation.cameraPosition, glm::vec3(0, 0, 0), glm::vec3(0.0f, 1.0f, 0.0f));
}

void mouse_call_back(GLFWwindow* window, double xPos, double yPos)
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

void process_input(GLFWwindow*& window)
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

int initialize_window(GLFWwindow*& window)
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
	glfwSetCursorPosCallback(window, mouse_call_back);
	glfwSetKeyCallback(window, key_call_back);

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

void create_cube(GLuint& VAO, GLuint& EBO, int& size, int& numIndices)
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

	int stride = (17) * sizeof(float);

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

static inline glm::vec3 get_vertex(const std::vector<float>& vertices, const int width, const int x, const int z, const int stride) {
	int index = (z * width + x) * stride;
	return glm::vec3(vertices[index], vertices[index + 1], vertices[index + 2]);
}

//This function has been made with the help of chat gpt and adjusted later.
static void calculate_normals(std::vector<float>& vertices, const int stride, const int height, const int width)
{
	for (unsigned int i = 0; i < (height - 1) * (width - 1); ++i)
	{
		int x = i % width;
		int z = i / width;

		glm::vec3 sum(0.0f);

		glm::vec3 center = get_vertex(vertices, width, x, z, stride);

		if (x > 0 && z > 0) {
			glm::vec3 left = get_vertex(vertices, width, x - 1, z, stride);
			glm::vec3 down = get_vertex(vertices, width, x, z - 1, stride);
			sum += glm::normalize(glm::cross(down - center, left - center));
		}
		if (x < width - 1 && z > 0) {
			glm::vec3 right = get_vertex(vertices, width, x + 1, z, stride);
			glm::vec3 down = get_vertex(vertices, width, x, z - 1, stride);
			sum += glm::normalize(glm::cross(right - center, down - center));
		}
		if (x > 0 && z < height - 1) {
			glm::vec3 left = get_vertex(vertices, width, x - 1, z, stride);
			glm::vec3 up = get_vertex(vertices, width, x, z + 1, stride);
			sum += glm::normalize(glm::cross(left - center, up - center));
		}
		if (x < width - 1 && z < height - 1) {
			glm::vec3 right = get_vertex(vertices, width, x + 1, z, stride);
			glm::vec3 up = get_vertex(vertices, width, x, z + 1, stride);
			sum += glm::normalize(glm::cross(up - center, right - center));
		}

		glm::vec3 normal = glm::normalize(sum);

		int vertexIndex = (i)*stride + 3;
		vertices[vertexIndex] = normal.x;
		vertices[vertexIndex + 1] = normal.y;
		vertices[vertexIndex + 2] = normal.z;
	}
}

void process_plane(const glm::vec2 currentChunkCord, const glm::vec3 position, const std::vector<unsigned int>& indices, const std::vector<float>& vertices)
{
	const int stride = 8;
	unsigned int VAO, VBO, EBO;

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * stride, 0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * stride, (void*)(sizeof(float) * 3));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * stride, (void*)(sizeof(float) * 6));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	Plane plane{};

	plane.VAO = VAO;
	plane.EBO = EBO;
	plane.vertices = vertices;
	plane.indices = indices;
	plane.position = position;

	//Replace the place holder placed during the dispatch, with the generated plane.
	activeTerrainChunks.insert_or_assign(currentChunkCord, std::move(plane));
}

void generate_landscape_chunk(const glm::vec2 currentChunkCord, const glm::vec3 position, const int size, float hScale, float xzScale, glm::vec3 offset, int concurrencyLevel)
{
	const int stride = 8;
	int count = size * size;

	const int gridSize = 400;
	const int octaves = 8;

	std::vector<float> vertices(count * stride);
	std::vector<unsigned int> indices((size - 1) * (size - 1) * 6);

	//Calculate Batch Size based on concurrency level.
	int threadCount = concurrencyLevel < 1 || concurrencyLevel > systemThreadsCount - 1 ? systemThreadsCount - 1 : concurrencyLevel;
	int batchSize = count / threadCount;
	int batches = count / batchSize;

	std::vector<std::thread> threads;

	//Process Primary Batches.
	for (int i = 0; i < batches; i++)
	{
		int start = i * batchSize;

		threads.emplace_back([=, &vertices]()
			{
				int vertexIndex = start * 8;

				for (int loopIndex = start; loopIndex < start + batchSize; ++loopIndex)
				{
					int x = loopIndex % size;
					int z = loopIndex / size;

					float globalX = x * xzScale;
					float globalZ = z * xzScale;

					vertices[vertexIndex++] = globalX;
					float perlinValue = perlin_noise::octaved_perlin_noise(globalX + offset.x, globalZ + offset.z, octaves, gridSize);

					vertices[vertexIndex++] = perlinValue * hScale;
					vertices[vertexIndex++] = globalZ;

					vertices[vertexIndex++] = 0.0f;
					vertices[vertexIndex++] = 0.0f;
					vertices[vertexIndex++] = 0.0f;

					vertices[vertexIndex++] = x / (float)size;
					vertices[vertexIndex++] = z / (float)size;
				}
			});
	}

	//Process Remainder while other batches are being processed.
	int remaining = count % threadCount;

	if (remaining > 0)
	{
		int start = count - remaining;
		int vertexIndex = start * 8;

		for (int i = start; i < start + remaining; ++i)
		{
			int x = i % size;
			int z = i / size;

			float globalX = x * xzScale;
			float globalZ = z * xzScale;

			vertices[vertexIndex++] = globalX;
			float perlinValue = perlin_noise::octaved_perlin_noise(globalX + offset.x, globalZ + offset.z, octaves, gridSize);

			vertices[vertexIndex++] = perlinValue * hScale;
			vertices[vertexIndex++] = globalZ;

			glm::vec3 cross = glm::cross(glm::vec3(1), glm::vec3(1));

			vertices[vertexIndex++] = 0.0f;
			vertices[vertexIndex++] = 0.0f;
			vertices[vertexIndex++] = 0.0f;

			vertices[vertexIndex++] = x / (float)size;
			vertices[vertexIndex++] = z / (float)size;
		}
	}

	for (auto& thread : threads)
	{
		thread.join();
	}
	threads.clear();

	unsigned int index = 0;
	for (unsigned int i = 0; i < (size - 1) * (size - 1); ++i)
	{
		int x = i % (size - 1);
		int z = i / (size - 1);

		int vertex = z * size + x;

		indices[index++] = vertex;
		indices[index++] = vertex + size;
		indices[index++] = vertex + size + 1;
		indices[index++] = vertex;
		indices[index++] = vertex + size + 1;
		indices[index++] = vertex + 1;
	}

	calculate_normals(vertices, stride, size, size);

	//Deffer finalization to the main thread.
	ActionQueue::shared_instance().AddActionToQueue([=, indices = std::move(indices), vertices = std::move(vertices)]() mutable
		{
			process_plane(currentChunkCord, position, indices, vertices);
		});
}