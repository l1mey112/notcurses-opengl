#include <notcurses/notcurses.h>
#include <assert.h>

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
	const struct ncvisual_options opts = {
		.n = ncr->pl,
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
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

int main(void)
{
	Display *display = NULL;

	static const int fb_config_attribs[] = {None};
	static const int context_attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 0,
		None};
	static const int pbuffer_attribs[] = {
		GLX_PBUFFER_WIDTH, 640,
		GLX_PBUFFER_HEIGHT, 480,
		None};

	int nitems;

	typedef GLXContext (*glXCreateContextAttribsARBProc)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
	typedef Bool (*glXMakeContextCurrentARBProc)(Display *, GLXDrawable, GLXDrawable, GLXContext);

	GLXFBConfig *fb_config = NULL;
	GLXContext context = NULL;
	GLXPbuffer pbuffer = 0;

	glXCreateContextAttribsARBProc glXCreateContextAttribsARB;
	glXMakeContextCurrentARBProc glXMakeContextCurrentARB;

	if (
		!(display = XOpenDisplay(NULL)) ||
		!(fb_config = glXChooseFBConfig(display, DefaultScreen(display), fb_config_attribs, &nitems)) ||
		!(glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((GLubyte *)"glXCreateContextAttribsARB")) ||
		!(glXMakeContextCurrentARB = (glXMakeContextCurrentARBProc)glXGetProcAddressARB((GLubyte *)"glXMakeContextCurrent")) ||
		!(context = glXCreateContextAttribsARB(display, fb_config[0], 0, True, context_attribs)) ||
		!(pbuffer = glXCreatePbuffer(display, fb_config[0], pbuffer_attribs)))
		return 1;

	XFree(fb_config);
	fb_config = NULL;

	XSync(display, False);

	// Bind context.
	if (!glXMakeContextCurrent(display, pbuffer, pbuffer, context))
	{
		glXDestroyPbuffer(display, pbuffer);
		pbuffer = 0;

		if (!glXMakeContextCurrent(display, DefaultRootWindow(display), DefaultRootWindow(display), context))
			return 1;
	}


}