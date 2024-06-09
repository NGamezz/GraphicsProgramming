#version 330 core
out vec4 FragColor;

in vec4 worldPosition;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;

uniform vec3 topColor;
uniform vec3 botColor;

uniform vec3 sunColor;

void main()
{
	vec3 viewDirection = normalize(worldPosition.rgb - cameraPosition);

	float sun = max(pow(dot(-viewDirection, lightDirection), 128), 0.0);

	FragColor = vec4(mix(botColor, topColor, max(viewDirection.y, 0.0)) + sun * sunColor, 1);
}