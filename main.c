#include <notcurses/notcurses.h>
#include <assert.h>

typedef struct{
	struct notcurses* nc;
	struct ncplane* pl;
	ncblitter_e nb;

	uint32_t *fb, fb_r_x, fb_r_xl, fb_r_y, fb_x, fb_y;
} NCRenderer;

static inline size_t ncr_sizeof_fb(NCRenderer *ncr) {
	return ncr->fb_r_x * ncr->fb_r_y * sizeof(uint32_t);
}

void ncr_recalculate_dimensions(NCRenderer *ncr) {
	ncr->fb_r_x = ncr->fb_x;
	ncr->fb_r_y = ncr->fb_y;

	switch (ncr->nb) {
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

static inline float ncr_aspect(NCRenderer *ncr) {
	return (float)ncr->fb_x / (float)ncr->fb_y;
}

void ncr_fullscreen(NCRenderer *ncr) {
	uint32_t ofb_x, ofb_y;
	ncplane_dim_yx(ncr->pl, &ofb_y, &ofb_x);

	if ( ofb_x != ncr->fb_x || ofb_y != ncr->fb_y ) {
		ncr->fb_x = ofb_x;
		ncr->fb_y = ofb_y; 
		ncr_recalculate_dimensions(ncr);
		
		if (ncr->fb) free(ncr->fb);
		
		ncr->fb = malloc(ncr_sizeof_fb(ncr));
		
		assert (ncr->fb);
	}
}

NCRenderer *ncr_init(ncblitter_e nb) {
	NCRenderer *ncr = malloc(sizeof(NCRenderer));

	ncr->nc = notcurses_init(NULL, stdout);
	ncr->pl = notcurses_stdplane(ncr->nc);
	ncr->nb = nb;

	ncr->fb = NULL;

	ncr_fullscreen(ncr);
	return ncr;
}

void ncr_blit(NCRenderer *ncr) {
	const struct ncvisual_options opts = {
		.n       = ncr->pl,
		.scaling = NCSCALE_NONE,
		.leny    = ncr->fb_r_y,
		.lenx    = ncr->fb_r_x,
		.blitter = ncr->nb
	};

	ncblit_rgba(ncr->fb, ncr->fb_r_xl, &opts);
	notcurses_render(ncr->nc);
}

int main() {
	NCRenderer *ncr = ncr_init(NCBLIT_1x1);

	for(;;) {
		ncr_fullscreen(ncr);

		memset(ncr->fb, 255, ncr_sizeof_fb(ncr));

		ncr_blit(ncr);
	};
	return 0;
}
