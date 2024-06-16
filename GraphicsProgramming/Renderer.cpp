#include "Renderer.h"

void Renderer::Intialize(GLuint& program)
{
	createProgram(program, "Resources/Shaders/simpleVertexShader.glsl", "Resources/Shaders/simpleFragmentShader.glsl");

	glUseProgram(program);
	glUniform1i(glGetUniformLocation(program, "mainTex"), 0);
	glUniform1i(glGetUniformLocation(program, "normalTex"), 1);
}

void Renderer::render_cube(unsigned int& cubeProgram, WorldInformation& worldInformation, Cube& cube)
{
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH);
	glEnable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);

	glUseProgram(cubeProgram);

	glm::mat4 world = glm::mat4(1.0f);
	world = glm::translate(world, glm::vec3(0, 100, 0));
	world = world * glm::mat4_cast(glm::quat(glm::vec3(0, 0.5f, 0)));
	world = glm::scale(world, glm::vec3(50));

	process_uniforms(cubeProgram, worldInformation, world);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, cube.Textures[0]);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, cube.Textures[1]);

	glBindVertexArray(cube.VAO);
	glDrawElements(GL_TRIANGLES, cube.IndexSize, GL_UNSIGNED_INT, 0);

	glUseProgram(0);
}

void Renderer::createProgram(GLuint& programId, const char* vertex, const char* fragment)
{
	auto vertexFuture = FileLoader::load_file_async(vertex);
	auto fragmentFuture = FileLoader::load_file_async(fragment);

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

void Renderer::render_skybox(unsigned int& skyProgram, WorldInformation& worldInformation, unsigned int skyboxVao, unsigned int skyBoxIndexSize)
{
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(skyProgram);

	glm::mat4 world = glm::mat4(1.0f);
	world = glm::translate(world, worldInformation.cameraPosition);
	world = glm::scale(world, glm::vec3(100.0f, 100.0f, 100.0f));

	process_uniforms(skyProgram, worldInformation, world);

	glBindVertexArray(skyboxVao);
	glDrawElements(GL_TRIANGLES, skyBoxIndexSize, GL_UNSIGNED_INT, 0);

	glEnable(GL_DEPTH);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glUseProgram(0);
}

void Renderer::render_model(Model* model, unsigned int program, WorldInformation worldInformation, glm::vec3 pos, glm::vec3 rotation, glm::vec3 scale)
{
	//glEnable(GL_BLEND);
	//Alpha Blend.
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//Additive
	//glBlendFunc(GL_ONE, GL_ONE);
	//Sort Additive
	//glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
	//Multiply
	//glBlendFunc(GL_DST_COLOR, GL_ZERO);
	//Double Multiply
	//glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH);
	glEnable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);

	glUseProgram(program);

	glm::mat4 world = glm::mat4(1.0f);
	world = glm::translate(world, pos);
	world = world * glm::mat4_cast(glm::quat(rotation));
	world = glm::scale(world, scale);

	process_uniforms(program, worldInformation, world);
	model->Draw(program);

	glDisable(GL_BLEND);
}

void Renderer::process_uniforms(unsigned int& program, WorldInformation& worldInformation, glm::mat4& worldMatrix)
{
	glUniformMatrix4fv(glGetUniformLocation(program, "world"), 1, GL_FALSE, glm::value_ptr(worldMatrix));
	//V = value pointer, sturen referentie naar instance door.
	glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(worldInformation.projection));
	glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, glm::value_ptr(worldInformation.view));

	glUniform3fv(glGetUniformLocation(program, "sunColor"), 1, glm::value_ptr(sunColor));

	glUniform3fv(glGetUniformLocation(program, "topColor"), 1, glm::value_ptr(topColor));
	glUniform3fv(glGetUniformLocation(program, "botColor"), 1, glm::value_ptr(botColor));

	glUniform3fv(glGetUniformLocation(program, "lightDirection"), 1, glm::value_ptr(worldInformation.lightPosition));
	glUniform3fv(glGetUniformLocation(program, "cameraPosition"), 1, glm::value_ptr(worldInformation.cameraPosition));
}

void Renderer::render_plane(unsigned int& planeProgram, Plane& plane, WorldInformation& worldInformation)
{
	glEnable(GL_DEPTH);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glUseProgram(planeProgram);

	glm::mat4 world = glm::mat4(1.0f);
	world = glm::translate(world, plane.position);

	process_uniforms(planeProgram, worldInformation, world);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, dirt);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, sand);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, grass);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, rock);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, snow);

	glBindVertexArray(plane.VAO);
	glDrawElements(GL_TRIANGLES, plane.indices.size() * sizeof(unsigned int), GL_UNSIGNED_INT, 0);
	glUseProgram(0);
}