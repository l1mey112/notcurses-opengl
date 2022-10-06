#include <assert.h>
#include <notcurses/notcurses.h>

typedef struct
{
	struct notcurses *nc;
	struct ncplane *pl;
	ncblitter_e nb;

	uint32_t *fb, fb_r_x, fb_r_xl, fb_r_y, fb_x, fb_y;
} NCRenderer;

static inline size_t ncr_sizeof_fb(NCRenderer *ncr)
{
	return ncr->fb_r_x * ncr->fb_r_y * sizeof(uint32_t);
}

void ncr_recalculate_dimensions(NCRenderer *ncr)
{
	ncr->fb_r_x = ncr->fb_x;
	ncr->fb_r_y = ncr->fb_y;

	switch (ncr->nb)
	{
	case NCBLIT_1x1:
		break;
	case NCBLIT_2x1:
		ncr->fb_r_y *= 2;
		break;
	case NCBLIT_2x2:
		ncr->fb_r_x *= 2;
		ncr->fb_r_y *= 2;
		break;
	case NCBLIT_3x2:
		ncr->fb_r_x *= 2;
		ncr->fb_r_y *= 3;
		break;
	case NCBLIT_BRAILLE:
		ncr->fb_r_x *= 2;
		ncr->fb_r_y *= 4;
		break;
	case NCBLIT_PIXEL:
	case NCBLIT_DEFAULT:
	case NCBLIT_4x1:
	case NCBLIT_8x1:
		break;
	}

	ncr->fb_r_xl = ncr->fb_r_x * sizeof(uint32_t);
}

static inline float ncr_aspect(NCRenderer *ncr)
{
	return (float)ncr->fb_x / (float)ncr->fb_y;
}

void ncr_fullscreen(NCRenderer *ncr)
{
	uint32_t ofb_x, ofb_y;
	ncplane_dim_yx(ncr->pl, &ofb_y, &ofb_x);

	if (ofb_x != ncr->fb_x || ofb_y != ncr->fb_y)
	{
		ncr->fb_x = ofb_x;
		ncr->fb_y = ofb_y;
		ncr_recalculate_dimensions(ncr);

		if (ncr->fb)
			free(ncr->fb);

		ncr->fb = malloc(ncr_sizeof_fb(ncr));

		assert(ncr->fb);
	}
}

NCRenderer *ncr_init(ncblitter_e nb)
{
	NCRenderer *ncr = malloc(sizeof(NCRenderer));

	ncr->nc = notcurses_init(NULL, stdout);
	ncr->pl = notcurses_stdplane(ncr->nc);
	ncr->nb = nb;

	ncr->fb = NULL;

	ncr_fullscreen(ncr);
	return ncr;
}

void ncr_blit(NCRenderer *ncr)
{
	const struct ncvisual_options opts = {.n = ncr->pl,
										  .scaling = NCSCALE_NONE,
										  .leny = ncr->fb_r_y,
										  .lenx = ncr->fb_r_x,
										  .blitter = ncr->nb};

	ncblit_rgba(ncr->fb, ncr->fb_r_xl, &opts);
	notcurses_render(ncr->nc);
}

int main1(void)
{
	NCRenderer *ncr = ncr_init(NCBLIT_1x1);

	for (;;)
	{
		ncr_fullscreen(ncr);

		memset(ncr->fb, 255, ncr_sizeof_fb(ncr));

		ncr_blit(ncr);
	};

	return 0;
}

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define SHADER(...) #__VA_ARGS__

#define GL_ASSERT(_str)                       \
	if (glGetError())                         \
	{                                         \
		fprintf(stderr, "ERROR: %s\n", _str); \
		exit(1);                              \
	}

#define GENERIC_ASSERT(_expression, _str)     \
	if (!(_expression))                       \
	{                                         \
		fprintf(stderr, "ERROR: %s\n", _str); \
		exit(1);                              \
	}

#include <sys/stat.h>

char *read_file(const char* filename) {
	FILE *pg = fopen(filename, "r");

	GENERIC_ASSERT(pg != NULL, "Failed to locate and open the program file")

	struct stat st;
	fstat(fileno(pg), &st);
	size_t pg_size = st.st_size;

	char *pgbuf = malloc(pg_size + 1);
	pgbuf[pg_size] = 0;

	fread(pgbuf, 1, pg_size, pg);
	fclose(pg);

	return pgbuf;
}

GLuint make_shader(const char *src, GLenum shader_type)
{
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, &src, NULL); // NULL is nul terminated
	glCompileShader(shader);

	GLint err;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &err);

	if (!err)
	{
		GLchar cout[1024];
		fprintf(stderr, "ERROR: Failed to compile shader");

		glGetShaderInfoLog(shader, 1023, NULL, cout);
		fprintf(stderr, "%s\n", cout);
		exit(1);
	}

	return shader;
}

// Warning: will delete both shaders
GLuint link_shaders(GLuint vert, GLuint frag, GLuint fbo)
{
	GLuint shader_program = glCreateProgram();
	glAttachShader(shader_program, vert);
	glAttachShader(shader_program, frag);
	glBindFragDataLocation(shader_program, fbo, "FragColor");

	glLinkProgram(shader_program);

	GLint err;

	glGetProgramiv(shader_program, GL_LINK_STATUS, &err);

	if (!err)
	{
		GLchar cout[1024];
		fprintf(stderr, "ERROR: Failed to link compute shader program");

		glGetShaderInfoLog(shader_program, 1023, NULL, cout);
		fprintf(stderr, "%s\n", cout);
		exit(1);
	}

	glDeleteShader(vert);
	glDeleteShader(frag);

	return shader_program;
}

GLuint createtexture(uint32_t x, uint32_t y, void *data)
{
	GLuint texture;
	glGenTextures(1, &texture);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, x, y, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, data);
	GL_ASSERT("Failed to create texture");

	glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32UI);
	GL_ASSERT("Failed to bind texture");

	return texture;
}

#include <assert.h>

typedef struct
{
	union
	{
		struct
		{
			float x, y, z;
		};
		float xyz[3];
	};
	union
	{
		struct
		{
			float u, v;
		};
		float uv[2];
	};
} vertex_t;

const vertex_t vertices[] = {
	{{{3.0, -1.0, 0.5}}, {{2.0, 0.0}}},
	{{{-1.0, 3.0, 0.5}}, {{0.0, 2.0}}},
	{{{-1.0, -1.0, 0.5}}, {{0.0, 0.0}}},
};
// fullscreen triangle (not quad), with correct UVs
// marcher-engine-gpu.git/main.v:123:2

int main(void)
{
	if (!glfwInit())
	{
		fprintf(stderr, "ERROR: Failed to initialise GLFW\n");
		return 1;
	}

	glfwWindowHint(GLFW_VISIBLE, /* GLFW_FALSE */ GLFW_TRUE);
	GLFWwindow *offscreen_context = glfwCreateWindow(640, 480, "", NULL, NULL);
	if (!offscreen_context)
	{
		fprintf(stderr, "ERROR: Failed to create a fake window\n");
		return 1;
	}
	glfwMakeContextCurrent(offscreen_context);

	if (glewInit())
	{
		fprintf(stderr, "ERROR: Failed to initialise GLEW\n");
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(offscreen_context);

	printf(
		"Vendor: %s\n"
		"Renderer: %s\n"
		"Version: %s\n"
		"Shader language: %s\n\n",
		glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION),
		glGetString(GL_SHADING_LANGUAGE_VERSION));

	GLuint vert = make_shader("#version 330 core\n" SHADER(
								  layout (location = 0) in vec3 pos;
								  layout (location = 1) in vec2 texcoord;
								  out vec2 uv;
								  void main() {
									  gl_Position = vec4(pos.x, pos.y, pos.z, 1.0);
									  uv = texcoord;
								  }),
							  GL_VERTEX_SHADER);

	GLuint frag = make_shader("#version 330 core\n" SHADER(
								  out vec4 FragColor;
								  in vec2 uv;
								  void main() {
									  FragColor = vec4(uv, 1.0f, 1.0f);
								  }),
							  GL_FRAGMENT_SHADER);

	GLuint shader_program = link_shaders(vert, frag, 0);

	GLuint VBO;
	glGenBuffers(1, &VBO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	GLuint VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void *)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void *)(sizeof(vertices->xyz)));
	glEnableVertexAttribArray(1);

	while (!glfwWindowShouldClose(offscreen_context))
	{
		glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shader_program);

		glDrawArrays(GL_TRIANGLES, 0, 3);
		glfwSwapBuffers(offscreen_context);
	}

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(shader_program);

	glfwTerminate();
}