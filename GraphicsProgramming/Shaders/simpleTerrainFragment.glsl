#version 330 core
out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec3 worldPosition;

uniform sampler2D mainTex;
uniform sampler2D normalTex;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;

void main()
{
	//Normals
	//vec3 normal = texture(normalTex, uv).rgb;
	//normal = normalize(normal * -2.0 + 1.0);

	//Specular Data
	//vec3 viewDirection = normalize(worldPosition - cameraPosition);
	//vec3 reflDir = normalize(reflect(lightDirection, normal));

	float light = max(-dot(normal, lightDirection), 0.0);
	//float specular = pow(max(-dot(reflDir, normalize(viewDirection)), 0.0f), 64);

	vec4 output = texture(mainTex, uv);
	output.rgb = output.rgb * min(light + 0.1f, 1.0f);// + specular * output.rgb;

	FragColor = output;
}