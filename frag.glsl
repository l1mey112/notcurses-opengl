#version 330 core

out vec4 fragment;
in vec2 uv;

void main() {
	fragment = vec4(uv, 0.0f, 1.0f);
}