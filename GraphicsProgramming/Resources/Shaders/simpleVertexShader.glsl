#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUv;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in vec3 vTangent;
layout(location = 5) in vec3 vBiTangent;

out vec3 color;
out vec2 uv;
out mat3 tbn;
out vec3 worldPosition;

uniform mat4 world, view, projection;

void main()
{
	//Object naar world, naar camera, naar clip space.
	vec3 pos = aPos;
	vec4 worldPos = world * vec4(pos, 1.0f);

	gl_Position = projection * view * worldPos;
	color = vColor;

	vec3 t = normalize(mat3(world) * vTangent);
	vec3 b = normalize(mat3(world) * vBiTangent);
	vec3 n = normalize(mat3(world) * vNormal);

	tbn = mat3(t, b, n);

	uv = vUv;
	worldPosition = mat3(world) * aPos;
}