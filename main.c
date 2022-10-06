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

#define INFO_MSG(strerr) fprintf(stderr, "[%s] %s\n", __func__, (strerr));

#define GENERIC_ASSERT(_err, strerr) \
	if (!(_err))                     \
	{                                \
		INFO_MSG((strerr))           \
		exit(1);                     \
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

GLuint createcomputeshader(const char *src)
{
	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
	glShaderSource(shader, 1, &src, NULL); // NULL is nul terminated
	glCompileShader(shader);

	GLint err;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &err);

	static GLchar cout[1024] = {0};
	if (!err)
	{
		fprintf(stderr, "ERROR: Failed to compile compute shader");

		glGetShaderInfoLog(shader, 1023, NULL, cout);
		fprintf(stderr, "%s\n", cout);
		exit(1);
	}

	GLuint program = glCreateProgram();
	glAttachShader(program, shader);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &err);

	if (!err)
	{
		fprintf(stderr, "ERROR: Failed to link compute shader program");

		glGetShaderInfoLog(shader, 1023, NULL, cout);
		fprintf(stderr, "%s\n", cout);
		exit(1);
	}

	glUseProgram(program);

	GL_ASSERT("Failed to use compute shader program");

	return program;
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

/* typedef struct
{
	float x, y, z;
	float u, v;
} vertex_t;

const vertex_t vertices[3] = {
	{3.0, -1.0, 0.5, 2.0, 0.0},
	{-1.0, 3.0, 0.5, 0.0, 2.0},
	{-1.0, -1.0, 0.5, 0.0, 0.0},
};
// fullscreen triangle (not quad), with correct UVs
// marcher-engine-gpu.git/main.v:123:2

GLuint getvbo()
{
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
} */

int main(void)
{
	if (!glfwInit())
	{
		fprintf(stderr, "ERROR: Failed to initialise GLFW\n");
		return 1;
	}

	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
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

	printf(
		"Vendor: %s\n"
		"Renderer: %s\n"
		"Version: %s\n"
		"Shader language: %s\n\n",
		glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION),
		glGetString(GL_SHADING_LANGUAGE_VERSION));

	/* GLuint shader = createcomputeshader("#version 430\n"SHADER(
		uniform writeonly image2D destTex;
		layout(local_size_x = 16, local_size_y = 16) in;
		void main() {
			ivec2 storePos = ivec2(gl_GlobalInvocationID.xy);
			imageStore(destTex, storePos, 1.0f, 1.0f, 1.0f, 1.0f);
		}
	)); */

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 800, 600, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	GENERIC_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
				   "Framebuffer is not complete");

	//	glDeleteTextures(1, &texColorBuffer);
	glfwTerminate();
}