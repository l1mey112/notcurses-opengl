#include <notcurses/notcurses.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <sys/stat.h>

#define CLOCK_MONOTONIC 1
#define ARROW_KEY_OFFSET 0.05f

typedef struct
{
	struct notcurses *nc;
	struct ncplane *pl;
	ncblitter_e nb;
	float f_aspect_x, f_aspect_y;

	uint32_t *fb, fb_r_x, fb_r_xl, fb_r_y, fb_x, fb_y;

	GLuint fbo, fbtex;
	GLFWwindow *offscreen_ctx;
} NCRenderer;

static inline size_t ncr_sizeof_fb(NCRenderer *ncr)
{
	return ncr->fb_r_x * ncr->fb_r_y * sizeof(uint32_t);
}

void ncr_recalculate_dimensions(NCRenderer *ncr)
{
	ncr->fb_r_x = ncr->fb_x;
	ncr->fb_r_y = ncr->fb_y;
	ncr->f_aspect_x = ncr->f_aspect_y = 1.0f;

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
		ncr->f_aspect_x = 2.0f;
		ncr->f_aspect_y = 2.0f;
		break;
	case NCBLIT_3x2:
		ncr->fb_r_x *= 2;
		ncr->fb_r_y *= 3;
		ncr->f_aspect_x = 1.5f;
		ncr->f_aspect_y = 1.0f;
		break;
	case NCBLIT_BRAILLE:
		ncr->fb_r_x *= 2;
		ncr->fb_r_y *= 4;
		ncr->f_aspect_x = 1.0f;
		ncr->f_aspect_y = 1.0f;
		break;
	case NCBLIT_PIXEL:
	case NCBLIT_DEFAULT:
	case NCBLIT_4x1:
	case NCBLIT_8x1:
		break;
	}

	ncr->fb_r_xl = ncr->fb_r_x * sizeof(uint32_t);
}

typedef union
{
	struct
	{
		float x, y;
	};
	float xy[2];
} vec2_t;

static inline vec2_t ncr_aspect(NCRenderer *ncr)
{
	vec2_t ret;

	if (ncr->fb_x >= ncr->fb_y)
	{
		ret.x = (float)ncr->fb_x / (float)ncr->fb_y / 2.0f;
		ret.y = 1.0f;
	}
	else
	{
		ret.x = 1.0f;
		ret.y = (float)ncr->fb_y / (float)ncr->fb_x / 2.0f;
	}

	ret.x *= ncr->f_aspect_x;
	ret.y *= ncr->f_aspect_y;

	return ret;
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

		glViewport(0, 0, ncr->fb_r_x, ncr->fb_r_y);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ncr->fb_r_x, ncr->fb_r_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	}
}

#define GENERIC_ASSERT(_expression, _str)     \
	if (!(_expression))                       \
	{                                         \
		fprintf(stderr, "ERROR: %s\n", _str); \
		exit(1);                              \
	}

NCRenderer *ncr_init_opengl(void)
{
	NCRenderer *ncr = malloc(sizeof(NCRenderer));

	if (!glfwInit())
	{
		fprintf(stderr, "ERROR: Failed to initialise GLFW\n");
		exit(1);
	}

	glfwWindowHint(GLFW_VISIBLE, 0);
	GLFWwindow *offscreen_context = glfwCreateWindow(640, 480, "", NULL, NULL);
	if (!offscreen_context)
	{
		fprintf(stderr, "ERROR: Failed to create a fake window\n");
		exit(1);
	}

	glfwMakeContextCurrent(offscreen_context);

	if (glewInit())
	{
		fprintf(stderr, "ERROR: Failed to initialise GLEW\n");
		glfwTerminate();
		exit(1);
	}

	printf("Vendor: %s\n"
		   "Renderer: %s\n"
		   "Version: %s\n"
		   "Shader language: %s\n",
		   glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION),
		   glGetString(GL_SHADING_LANGUAGE_VERSION));

	int a;
	int b[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, (int *)&b, &a);
	printf("Floating point bits of precision: %d\n"
	       "Lowest value: -2e%d\n"
		   "Highest value: 2e%d\n", a, b[0], b[1]);
	// The base 2 log of the absolute value of the minimum/maximum value that can be represented.


	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	unsigned int fbtex;
	glGenTextures(1, &fbtex);
	glBindTexture(GL_TEXTURE_2D, fbtex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbtex, 0);

	GENERIC_ASSERT(glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
				   "Failed to create framebuffer object");

	ncr->fbo = fbo;
	ncr->fbtex = fbtex;
	ncr->offscreen_ctx = offscreen_context;

	return ncr;
}

#define NANOSECS_IN_SEC 1000000000
static inline uint64_t
timespec_to_ns(const struct timespec *ts)
{
	return ts->tv_sec * NANOSECS_IN_SEC + ts->tv_nsec;
}

void ncr_init_notcurses(NCRenderer *ncr, ncblitter_e nb)
{
	ncr->nc = notcurses_init(NULL, stdout);
	ncr->pl = notcurses_stdplane(ncr->nc);
	ncr->nb = nb;
	ncr->fb = NULL;

	ncr->fb_x = ncr->fb_y = -1;
	ncr_fullscreen(ncr);
}

void ncr_cleanup_opengl(NCRenderer *ncr)
{
	glDeleteTextures(1, &ncr->fbtex);
	glDeleteFramebuffers(1, &ncr->fbo);
	glfwTerminate();
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

void ncr_opengl_blit(NCRenderer *ncr)
{
	// glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glActiveTexture(GL_TEXTURE0);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, ncr->fb);
	// glReadPixels(0, 0, ncr->fb_r_x, ncr->fb_r_y, GL_RGBA, GL_UNSIGNED_BYTE, ncr->fb);

	// glfwSwapBuffers(ncr->offscreen_ctx);
}

char *read_file(const char *filename)
{
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
		fprintf(stderr, "ERROR: Failed to compile shader\n\n");

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
	glBindFragDataLocation(shader_program, fbo, "fragment");

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
	{{{3.0, 1.0, 0.5}}, {{2.0, 0.0}}},
	{{{-1.0, -3.0, 0.5}}, {{0.0, 2.0}}},
	{{{-1.0, 1.0, 0.5}}, {{0.0, 0.0}}},
};
// fullscreen triangle (not quad), with correct UVs
// marcher-engine-gpu.git/main.v:123:2
//
// flipped upside down because pixels taken from the framebuffer
// are inverted in opengl. if it was a normal window i would not
// have to do this at all.

int main(void)
{
	NCRenderer *ncr = ncr_init_opengl();

	GLuint vert = make_shader(read_file("vert.glsl"), GL_VERTEX_SHADER);
	GLuint frag = make_shader(read_file("frag.glsl"), GL_FRAGMENT_SHADER);
	GLuint shader_program = link_shaders(vert, frag, 0);
	glUseProgram(shader_program);

	int ul = glGetUniformLocation(shader_program, "u_aspect");
	assert(ul != -1);

	int ul1 = glGetUniformLocation(shader_program, "u_zoom");
	assert(ul1 != -1);

	int ul2 = glGetUniformLocation(shader_program, "u_offset");
	assert(ul2 != -1);

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

	ncr_init_notcurses(ncr, NCBLIT_2x2);

	assert(notcurses_mice_enable(ncr->nc, /* NCMICE_MOVE_EVENT |  */ NCMICE_BUTTON_EVENT) != -1);

	float zoom = 1.0f;
	vec2_t offset = {{0.0f, 0.0f}};

	uint32_t ch;
	for (;;)
	{
		if ((ch = notcurses_get_nblock(ncr->nc, NULL)) != 0)
		{
			if (nckey_synthesized_p(ch))
			{
				switch (ch)
				{
				case NCKEY_LEFT:
				{
					offset.x -= ARROW_KEY_OFFSET * zoom;
					break;
				}
				case NCKEY_RIGHT:
				{
					offset.x += ARROW_KEY_OFFSET * zoom;
					break;
				}
				case NCKEY_UP:
				{
					offset.y += ARROW_KEY_OFFSET * zoom;
					break;
				}
				case NCKEY_DOWN:
				{
					offset.y -= ARROW_KEY_OFFSET * zoom;
					break;
				}
				case NCKEY_BUTTON4: // SCROLL UP
				{
					zoom *= 1.1f;
					break;
				}
				case NCKEY_BUTTON5: // SCROLL DOWN
				{
					zoom /= 1.1f;
					break;
				}
				default:
					break;
				}
			}
		}

		ncr_fullscreen(ncr);

		vec2_t aspect = ncr_aspect(ncr);
		glUniform2f(ul, aspect.x, aspect.y);
		glUniform1f(ul1, zoom);
		glUniform2f(ul2, offset.x, offset.y);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		ncr_opengl_blit(ncr);

		ncr_blit(ncr);
	}

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteProgram(shader_program);
	ncr_cleanup_opengl(ncr);
}