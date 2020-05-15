#include <SDL_egl.h>
#include <SDL_opengles2.h>

#ifndef EGL_VERSION_1_5
#error EGLRenderer requires EGL 1.5
#endif

#ifndef GL_ES_VERSION_2_0
#error EGLRenderer requires OpenGL ES 2.0
#endif

int main() {
    return 0;
}