#version 330 core

out vec4 FragColor;
in vec3 color;
in vec2 uv;
in mat3 tbn;
in vec3 worldPosition;

uniform sampler2D mainTex;
uniform sampler2D normalTex;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;

uniform vec3 topColor;
uniform vec3 botColor;

uniform vec3 sunColor;

void main()
{
	//Normals
	vec3 normal = texture(normalTex, uv).rgb;
	normal = normalize(normal * -2.0 + 1.0);

	normal.rg = normal.rg;
	normal = normalize(normal);

	normal = tbn * normal;

	//Specular Data
	vec3 viewDirection = normalize(worldPosition - cameraPosition);
	vec3 reflDir = normalize(reflect(lightDirection, normal));

	float light = max(dot(normal, lightDirection), 0.0);
	float specular = pow(max(-dot(reflDir, normalize(viewDirection)), 0.0f), 128);

	float dist = length(worldPosition.rgb - cameraPosition);

	vec3 fogColor = mix(botColor, topColor, max(viewDirection.y, 0.0));

	const float fogMult = 1.0 / 1000.0;
	float fog = pow(clamp((dist - 250) * fogMult, 0.0, 1.0), 2.0);

	vec4 output = vec4(color, 1.0f) * texture(mainTex, uv);
	output.rgb = mix((output.rgb * sunColor * min(light + 0.2f, 1.0f) + specular), fogColor, fog);

	FragColor = output;
}