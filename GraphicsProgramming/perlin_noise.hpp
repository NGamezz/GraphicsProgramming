#pragma once
#include <corecrt_math.h>

class perlin_noise
{
public:
	static float octaved_perlin_noise(const float x, const float y, const int octaves, const int size);
	static glm::vec2 get_random_gradient(const int ix, const int iy);
	static float regular_perlin_noise(const float x, const float z);
	static float dot_grid_gradient(const int ix, const int iy, const float x, const float y);
	static float interpolate(const float a, const float b, const float value);
};

inline float perlin_noise::octaved_perlin_noise(const float x, const float y, const int octaves, const int size)
{
	float perlinValue = 0.0f;
	float frequency = 1.0f;
	float amplitude = 1.0f;

	float maxValue = 0.0f;

	for (int octave = 0; octave < octaves; ++octave)
	{
		float sampleX = x / size * frequency;
		float sampleY = y / size * frequency;

		float val = regular_perlin_noise(sampleX, sampleY) * amplitude;

		perlinValue += val;

		maxValue += amplitude;

		amplitude *= 0.5f;
		frequency *= 2.0f;
	}

	perlinValue = (perlinValue + 1) / maxValue;

	return perlinValue;
}

inline float perlin_noise::regular_perlin_noise(const float x, const float z)
{
	int xGrid = floor(x);
	int yGrid = floor(z);
	int xGridB = xGrid + 1;
	int yGridB = yGrid + 1;

	float sx = x - (float)xGrid;
	float sy = z - (float)yGrid;

	float n0 = dot_grid_gradient(xGrid, yGrid, x, z);
	float n1 = dot_grid_gradient(xGridB, yGrid, x, z);
	float ix0 = interpolate(n0, n1, sx);

	n0 = dot_grid_gradient(xGrid, yGridB, x, z);
	n1 = dot_grid_gradient(xGridB, yGridB, x, z);
	float ix1 = interpolate(n0, n1, sx);

	float value = interpolate(ix0, ix1, sy);

	return value;
}

//This Hashing function has been acquired from the internet.
inline glm::vec2 perlin_noise::get_random_gradient(const int ix, const int iy)
{
	// No precomputed gradients mean this works for any number of grid coordinates
	const unsigned int w = 8 * sizeof(unsigned);
	const unsigned int s = w / 2;
	unsigned int a = ix, b = iy;
	a *= 3284157443;

	b ^= a << s | a >> w - s;
	b *= 1911520717;

	a ^= b << s | b >> w - s;
	a *= 2048419325;
	float random = a * (3.14159265 / ~(~0u >> 1)); // in [0, 2*Pi]

	// Create the vector from the angle
	glm::vec2 v{};
	v.x = sin(random);
	v.y = cos(random);

	return v;
}

inline float perlin_noise::dot_grid_gradient(const int ix, const int iy, const float x, const float y)
{
	glm::vec2 gradient = perlin_noise::get_random_gradient(ix, iy);

	float dx = x - (float)ix;
	float dy = y - (float)iy;

	return (dx * gradient.x + dy * gradient.y);
}

inline float perlin_noise::interpolate(const float a, const float b, const float value)
{
	return (b - a) * (3.0 - value * 2.0) * value * value + a;
}