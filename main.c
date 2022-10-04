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

int main1()
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

#define CL_TARGET_OPENCL_VERSION 300
#include <CL/opencl.h>

#include <sys/stat.h>

#define INFO_MSG(strerr) fprintf(stderr, "[%s] %s\n", __func__, (strerr));

#define CL_ASSERT(strerr)  \
	if (err != CL_SUCCESS) \
	{                      \
		INFO_MSG((strerr)) \
		exit(1);           \
	}

#define GENERIC_ASSERT(_err, strerr) \
	if (!(_err))                     \
	{                                \
		INFO_MSG((strerr))           \
		exit(1);                     \
	}

cl_device_id create_device()
{
	cl_platform_id platform;
	cl_device_id dev;
	int err;

	/* Identify a platform */
	err = clGetPlatformIDs(1, &platform, NULL);
	CL_ASSERT("could not identify a platform")

	// Access a device
	// GPU
	err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
	if (err == CL_DEVICE_NOT_FOUND)
	{
		// CPU
		err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_CPU, 1, &dev, NULL);
		INFO_MSG("accessing cpu");
	}
	else
	{
		INFO_MSG("accessing gpu");
	}
	CL_ASSERT("could not get cl compatible device")

	//	// https://community.amd.com/t5/archives-discussions/opencl-optimal-work-group-size/td-p/220026
	//	// device max work group
	//	err = clGetDeviceInfo(dev, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(size_t), work_group_ptr, NULL);
	//	if (err != CL_SUCCESS) {
	//		fputs("could not get device information", stderr);
	//		exit(1);
	//	}
	//	fprintf(stderr, "CL_DEVICE_MAX_WORK_GROUP_SIZE = %zu\n", *work_group_ptr);

	return dev;
}

cl_program add_program(cl_context ctx, cl_device_id dev, const char *filename)
{
	FILE *pg = fopen(filename, "r");

	GENERIC_ASSERT(pg != NULL, "failed to locate and open the program file")

	struct stat st;
	fstat(fileno(pg), &st);

	char *pgbuf = malloc(st.st_size + 1);
	pgbuf[st.st_size] = 0;

	fread(pgbuf, 1, st.st_size, pg);
	fclose(pg);

	cl_int err;

	cl_program clpg = clCreateProgramWithSource(ctx, 1, (const char **)&pgbuf, &st.st_size, &err);
	CL_ASSERT("failed to create cl program")
	free(pgbuf);

	// https://rocmdocs.amd.com/en/latest/Programming_Guides/Opencl-programming-guide.html#compilin-host-program
	err = clBuildProgram(clpg, 0, NULL, NULL, NULL, NULL);
	if (err != CL_SUCCESS)
	{
		INFO_MSG("failed to compile cl program");

		size_t cout_size;
		err = clGetProgramBuildInfo(clpg, dev, CL_PROGRAM_BUILD_LOG, 0, NULL, &cout_size);
		CL_ASSERT("failed to get program compilation information")

		char *cout = malloc(cout_size + 1);
		cout[cout_size] = 0;

		clGetProgramBuildInfo(clpg, dev, CL_PROGRAM_BUILD_LOG, cout_size + 1, cout, NULL);
		CL_ASSERT("failed to get program compilation information")

		fputs(cout, stderr);

		free(cout);
		exit(1);
	}

	return clpg;
}

//    • Intel recommends workgroup size of 64-128. Often 128 is minimum to
//    get good performance on GPU
//    • On NVIDIA Fermi, workgroup size must be at least 192 for full
//    utilization of cores
//    • Optimal workgroup size differs across applications

// #define MSG(a, args...) fprintf(stderr, "[%s:%d: %s]: \x1b[92m"a"\x1b[39m\n",__FILE__,__LINE__,__func__,args)

#include <math.h>

int main(void)
{
	cl_int err;
	cl_device_id cldevice = create_device();

	cl_context clctx = clCreateContext(NULL, 1, &cldevice, NULL, NULL, &err);
	CL_ASSERT("could not create a cl context");
	cl_command_queue clqueue = clCreateCommandQueueWithProperties(clctx, cldevice, 0, &err);
	CL_ASSERT("could not create a cl command queue");

	cl_program clprogram = add_program(clctx, cldevice, "pg.cl");
	cl_kernel clkernel = clCreateKernel(clprogram, "vecAdd", &err);
	CL_ASSERT("could not create a cl kernel");

	// Length of vectors
	unsigned int n = 100000;
	// Host input vectors
	double *h_a;
	double *h_b;
	// Host output vector
	double *h_c;

	// Size, in bytes, of each vector
	size_t bytes = n * sizeof(double);

	// Allocate memory for each vector on host
	h_a = (double *)malloc(bytes);
	h_b = (double *)malloc(bytes);
	h_c = (double *)malloc(bytes);

	int i;
	for (i = 0; i < n; i++)
	{
		h_a[i] = sinf(i) * sinf(i);
		h_b[i] = cosf(i) * cosf(i);
	}

	size_t globalSize, localSize;
	// Number of work items in each local work group
	localSize = 64;

	// Number of total work items - localSize must be devisor
	globalSize = ceil(n / (float)localSize) * localSize;

	// Create the input and output arrays in device memory for our calculation
	// Device input buffers
	cl_mem d_a = clCreateBuffer(clctx, CL_MEM_READ_ONLY, bytes, NULL, NULL);
	cl_mem d_b = clCreateBuffer(clctx, CL_MEM_READ_ONLY, bytes, NULL, NULL);
	// Device output buffer
	cl_mem d_c = clCreateBuffer(clctx, CL_MEM_WRITE_ONLY, bytes, NULL, NULL);

	err = clEnqueueWriteBuffer(clqueue, d_a, CL_TRUE, 0,
							   bytes, h_a, 0, NULL, NULL);
	err |= clEnqueueWriteBuffer(clqueue, d_b, CL_TRUE, 0,
								bytes, h_b, 0, NULL, NULL);

	CL_ASSERT("could not create write buffers");

	// Set the arguments to our compute kernel
	err = clSetKernelArg(clkernel, 0, sizeof(cl_mem), &d_a);
	err |= clSetKernelArg(clkernel, 1, sizeof(cl_mem), &d_b);
	err |= clSetKernelArg(clkernel, 2, sizeof(cl_mem), &d_c);
	err |= clSetKernelArg(clkernel, 3, sizeof(unsigned int), &n);

	CL_ASSERT("could not set cl kernel arguments");

	err = clEnqueueNDRangeKernel(clqueue, clkernel, 1, NULL, &globalSize, &localSize,
								 0, NULL, NULL);

	CL_ASSERT("could not execute kernel");

	clFinish(clqueue);

	clEnqueueReadBuffer(clqueue, d_c, CL_TRUE, 0,
						bytes, h_c, 0, NULL, NULL);

	// Sum up vector c and print result divided by n, this should equal 1 within error
	double sum = 0;
	for (i = 0; i < n; i++)
		sum += h_c[i];
	printf("final result: %f\n", sum / n);

	clReleaseMemObject(d_a);
    clReleaseMemObject(d_b);
    clReleaseMemObject(d_c);
    clReleaseProgram(clprogram);
    clReleaseKernel(clkernel);
    clReleaseCommandQueue(clqueue);
    clReleaseContext(clctx);

	//release host memory
    free(h_a);
    free(h_b);
    free(h_c);

	return 0;
}