# notcurses-opengl

A project utilising notcurses and OpenGL to render to the terminal in an efficient manner. It uses a screen space shader to calculate the mandelbrot set (a re-occurring theme with my projects). notcurses is so fast that the GPU becomes the bottleneck when rendering!

Use this code as an example when doing OpenGL windowless rendering and for writing to the notcurses framebuffer.

Better yet, check out my post on the explanation of how this all comes together!

### [hijacking-opengl-with-notcurses](https://blog.l-m.dev/posts/hijacking-opengl-with-notcurses/)

#

Use the arrow keys to move around, mouse scroll to zoom in and out.

![notcurses-zoomed-out](/media/notcurses-zoomed-out.png)
![notcurses-gl-hellotriangle](/media/notcurses-gl-hellotriangle.png)
![notcurses-mandelbrot](/media/notcurses-mandelbrot.png)
![notcurses-well](/media/notcurses-well.png)

[`mirrored from my git instance....`](https://git.l-m.dev/l-m/notcurses-opengl)