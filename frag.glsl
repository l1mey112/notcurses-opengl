#version 330 core

out vec4 fragment;
in vec2 uv;
uniform vec2 aspect;

float sphereSDF(vec3 p, vec3 c, float r){
	return length(p - c) - r;
}

float f(vec3 p){
	return sphereSDF(p, vec3(0,0,10), 1.2f);
}

vec4 march() {
	vec2 ssp = uv * aspect;

	vec3 ro = vec3(0);
	vec3 rd = normalize(vec3(ssp - aspect / 2.0f, 1.5f));

	float ray_distance = 0.0f;

	const int max_march = 256;
	float smallest_distance = 0.0;
	for(int i = 0; i < max_march; i++){
		vec3 p = ro + rd * ray_distance;
		float min_distance = f(p);

		if(min_distance < 0.0001){
			// vec3 nrm = calcNormal(p);
			return vec4(1.0f, 1.0f, 1.0f, 1.0f);
		} else if (min_distance > 1000){
			break;
		}

		ray_distance += min_distance;
	}

	return vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

void main() {
	fragment = march();
}