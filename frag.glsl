#version 330 core

out vec4 fragment;
in vec2 uv;

float sphereSDF(vec3 p, vec3 c, float r){
    return length(p - c) - r;
}

float f(vec3 p){
	return sphereSDF(p, vec3(0), 1.2f);
}

vec4 march() {
	vec3 ro = vec3(0);
    vec3 rd = normalize(vec3(uv, 1.5f));

	return vec4(rd, 1);

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
	fragment = vec4(uv,0,1);
	/* if (uv.x >= 0.25 && uv.x <= 0.75 && uv.y >= 0.25 && uv.y <= 0.75) {
		fragment = vec4(uv,0,1);
	} else {
		fragment = vec4(0,0,0,1);
	} */
}