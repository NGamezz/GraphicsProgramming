#version 330 core
out vec4 FragColor;

in vec2 uv;
in vec3 normal;
in vec4 worldPosition;

uniform sampler2D dirt, sand, grass, rock, snow;

uniform vec3 lightDirection;
uniform vec3 cameraPosition;

uniform vec3 topColor;
uniform vec3 botColor;

uniform float nearField;
uniform float farField;

uniform vec3 sunColor;

void main()
{
	vec3 viewDirection = normalize(worldPosition.rgb - cameraPosition);
	vec3 reflDir = normalize(reflect(lightDirection, normal));

	float light = max(-dot(normal, lightDirection), 0.0f);
	float specular = pow(max(-dot(reflDir, viewDirection), 0.0f), 128);

	//color weights
	float y = worldPosition.y;
	float ds = clamp((y + 25) * 0.1f, 0.0f, 1.0f);
	float sg = clamp(y * 0.1f, 0.0f, 1.0);
	float gr = clamp((y - 25) * 0.1f, 0.0f, 1.0f);
	float rs = clamp((y - 50) * 0.1f, 0.0f, 1.0f);

	float dist = length(worldPosition.xyz - cameraPosition);

	const float uvMult = 1.0 / 150.0f;
	float uvLerp = clamp((dist - 250) * uvMult, 0.0f, 1.0f);

	// lerp between close and far colors in a loop
	vec3 colors[5];
	vec3 colorsClose[5] = vec3[](
		texture(dirt, uv * 100.0f).rgb,
		texture(sand, uv * 100.0f).rgb,
		texture(grass, uv * 100.0f).rgb,
		texture(rock, uv * 100.0f).rgb,
		texture(snow, uv * 100.0f).rgb
		);

	vec3 colorsFar[5] = vec3[](
		texture(dirt, uv * 10.0f).rgb,
		texture(sand, uv * 10.0f).rgb,
		texture(grass, uv * 10.0f).rgb,
		texture(rock, uv * 10.0f).rgb,
		texture(snow, uv * 10.0f).rgb
		);

	for (int i = 0; i < 5; ++i) {
		colors[i] = mix(colorsClose[i], colorsFar[i], uvLerp);
	}

	// Calculate fog color
	vec3 fogColor = mix(botColor, topColor, max(viewDirection.y, 0.0f));

	const float fogMult = 1.0f / 1000.0f;
	float fog = pow(clamp((dist - 250) * fogMult, 0.0f, 1.0f), 2.0f);

	// Blend the textures based on weights. Mix linearly interpolates.
	vec3 diffuse = mix(mix(mix(mix(colors[0] + specular * colors[0], colors[1], ds), colors[2], sg), colors[3] + specular * colors[3], gr), colors[4], rs);

	diffuse *= (sunColor * min(light + 0.2f, 1.0f));
	vec3 color = mix(diffuse, fogColor, fog);
	FragColor = vec4(color, 1.0f);
}