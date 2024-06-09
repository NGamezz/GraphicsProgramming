#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUv;

out vec2 uv;
out vec3 normal;
out vec4 worldPosition;

uniform sampler2D mainTex;

uniform mat4 world, view, projection;

void main()
{
	vec3 pos = aPos;

	vec4 worldPos = world * vec4(pos, 1.0);

	gl_Position = projection * view * worldPos;
	
	uv = vUv;
	normal = vNormal;

	worldPosition = worldPos;
}