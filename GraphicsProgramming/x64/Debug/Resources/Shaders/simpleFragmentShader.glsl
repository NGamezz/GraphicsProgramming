#version 330 core

out vec4 FragColor;
in vec3 color;
in vec2 uv;
in mat3 tbn;
in vec3 worldPosition;

uniform sampler2D mainTex;
uniform sampler2D normalTex;
uniform vec3 lightPosition;
uniform vec3 cameraPosition;

uniform vec3 topColor;
uniform vec3 botColor;

void main()
{
	//Normals
	vec3 normal = texture(normalTex, uv).rgb;
	normal = normalize(normal * -2.0 + 1.0);

	normal.rg = normal.rg * 2;
	normal = normalize(normal);

	normal = tbn * normal;

	//Lighting
	vec3 lightDirection = normalize(worldPosition - lightPosition);

	//Specular Data
	vec3 viewDirection = normalize(worldPosition - cameraPosition);
	vec3 reflDir = normalize(reflect(lightDirection, normal));

	float light = max(dot(normal, lightDirection), 0.0);
	float specular = pow(max(-dot(reflDir, normalize(viewDirection)), 0.0f), 64);

	vec4 output = vec4(color, 1.0f) * texture(mainTex, uv);
	output.rgb = output.rgb * min(light + 0.1f, 1.0f) + specular;

	FragColor = output;
}