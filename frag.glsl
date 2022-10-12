#version 330 core

precision highp float;

out vec4 fragment;
in vec2 uv;

uniform vec2 u_aspect;
uniform float u_zoom;
uniform vec2 u_offset;

const int max_iterations = 1024;

highp vec2 f(highp vec2 z) {
	return mat2(z.x, z.y, -z.y, z.x) * z;
}

int mandelbrot(vec2 coord)
{
	highp vec2 z = vec2(0, 0);
	int i;
	for (i = 0; i < max_iterations; i++)
	{
		z = f(z) + coord;

		if (length(z) > 2) {
			break;
		}
	}

	// https://en.wikipedia.org/wiki/Plotting_algorithms_for_the_Mandelbrot_set#Continuous_(smooth)_coloring

	if (i < max_iterations) {
		highp float log_zn = log(z.x * z.x + z.y * z.y) / 2.0;
		highp float nu = log(log_zn / log(2.0)) / log(2.0);
		i = i + 1 - int(nu);

		return i;
	} else {
		return max_iterations;
	}
}

// const float PI = 3.14159265359;

// vec3 hsv2rgb(vec3 c) {
//     vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
//     vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
//     return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
// }

vec3 palette(float t, vec3 c1, vec3 c2, vec3 c3, vec3 c4, vec3 c5) {
	float x = 1.0 / 4.0;
	if (t < x) return mix(c1, c2, t/x);
	else if (t < 2.0 * x) return mix(c2, c3, (t - x)/x);
	else if (t < 3.0 * x) return mix(c3, c4, (t - 2.0*x)/x);
	else if (t < 4.0 * x) return mix(c4, c5, (t - 3.0*x)/x);
	return c5;
}

vec3 get_colour(int cidx) {
	float v = float(cidx) / max_iterations;

//	v = 1.0 - pow(cos(PI * v), 2.0);
//	vec3 hsv;
//	hsv.x = mod(pow(v * 360.0, 1.5) + 2697.0, 360);
//	hsv.y = 50.0;
//	hsv.z = v * 100;
//	return hsv2rgb(hsv);

	const vec3 c1 = vec3(0.0, 0.0, 0.0);
	const vec3 c2 = vec3(0.24705882352941178, 0.10980392156862745, 0.10980392156862745);
	const vec3 c3 = vec3(0.054901960784313725, 0.2, 0.5686274509803921);
	const vec3 c4 = vec3(0.984313725490196, 0.7333333333333333, 0.2);
	const vec3 c5 = vec3(1.0, 1.0, 1.0);
	return palette(v, c1, c4, c1, c4, c1);
}

void main()
{
	vec2 ssp = uv * u_aspect - u_aspect / 2.0;
	vec2 imaginarycoord = ssp * 2.0;

	imaginarycoord *= u_zoom;
	imaginarycoord += u_offset;

	int cidx = mandelbrot(imaginarycoord);

	fragment = vec4(get_colour(cidx), 1);
}