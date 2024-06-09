#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 Normals;
in vec4 FragPos;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;
uniform sampler2D texture_normal1;
uniform sampler2D texture_roughness1;
uniform sampler2D texture_ao1;

uniform vec3 cameraPosition;
uniform vec3 lightDirection;

uniform vec3 topColor;
uniform vec3 botColor;

uniform vec3 sunColor;

void main()
{
	vec4 diffuse = texture(texture_diffuse1, TexCoords);
	vec4 specTex = texture(texture_specular1, TexCoords);

	float light = max(-dot(lightDirection, Normals), 0.0);

	vec3 viewDir = normalize(FragPos.rgb - cameraPosition);
	vec3 refl = reflect(lightDirection, Normals);

	float ambientOcclusion = texture(texture_ao1, TexCoords).r;

	// Calculate fog color
	vec3 fogColor = mix(botColor, topColor, max(viewDir.y, 0.0));

	float dist = length(FragPos.xyz - cameraPosition);

	const float fogMult = 1.0 / 1000.0;
	float fog = pow(clamp((dist - 250) * fogMult, 0.0, 1.0), 2.0);

	float roughness = texture(texture_roughness1, TexCoords).r;
	float spec = pow(max(-dot(viewDir, refl), 0.0), mix(1, 128, roughness));
	vec3 specular = spec * specTex.rgb;

	vec4 output = mix(diffuse * vec4(sunColor, 1.0f) * max(light * ambientOcclusion, 0.2 * ambientOcclusion) + vec4(specular, 0), vec4(fogColor, 1.0f), fog);

	//Clip at treshhold.
	//if (output.a < 0.5) discard;

	FragColor = output;
}