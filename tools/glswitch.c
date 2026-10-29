// tools/glswitch.c (M3-R4 mode-switch-fix, docs/m3-r4-mode-switch-fix.md): the engine's window re-creation on a resolution
// switch, stand-alone, on the real X server with HIDDEN windows (never mapped: nothing on screen, no input).
//   cc -O0 -g -o glswitch tools/glswitch.c $(sdl2-config --cflags --libs) -lGL
//   SDL_VIDEODRIVER=x11 timeout -s KILL 25 ./glswitch old|new [swap interval 0|1]
// old = GoK's rndClose (SDL_QuitSubSystem(VIDEO), the context leaked): BadPixmap (interval 0) or a hung swap (1) under
// the NVIDIA driver and Xwayland; new = the fix (context deleted, window destroyed, the connection kept): 4 windows ok.
// SDL turns SIGTERM into SDL_QUIT, which a hung swap never reads: always time out with SIGKILL.
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <stdio.h>
#include <string.h>
static int xerrs = 0;
static int round_(int r, int w, int h, int interval, int hidden, SDL_Window **win, SDL_GLContext *ctx)
{
    int i, dw, dh;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { printf("init: %s\n", SDL_GetError()); return 0; }
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    *win = SDL_CreateWindow("glswitch", 0, 0, w, h, SDL_WINDOW_OPENGL | (hidden ? SDL_WINDOW_HIDDEN : 0));
    if (!*win) { printf("window: %s\n", SDL_GetError()); return 0; }
    *ctx = SDL_GL_CreateContext(*win);
    if (!*ctx) { printf("context: %s\n", SDL_GetError()); return 0; }
    SDL_GL_MakeCurrent(*win, *ctx);
    SDL_GL_SetSwapInterval(interval);
    SDL_GL_GetDrawableSize(*win, &dw, &dh);
    for (i = 0; i < 30; i++)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        glViewport(0, 0, dw, dh);
        glClearColor(0.1f * r, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        SDL_GL_SwapWindow(*win);
    }
    glFinish();
    printf("round %d: %dx%d drawable %dx%d renderer '%s' 30 swaps ok\n", r, w, h, dw, dh, (const char *)glGetString(GL_RENDERER));
    fflush(stdout);
    return 1;
}
int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "old";
    int interval = argc > 2 ? atoi(argv[2]) : 1;
    int hidden = 1, r;
    SDL_Window *win = NULL; SDL_GLContext ctx = NULL;
    static const int sizes[4][2] = { { 1024, 768 }, { 3840, 2160 }, { 2560, 1600 }, { 3840, 2160 } };
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("mode %s interval %d driver %s\n", mode, interval, getenv("SDL_VIDEODRIVER") ? getenv("SDL_VIDEODRIVER") : "-");
    for (r = 0; r < 4; r++)
    {
        if (!round_(r, sizes[r][0], sizes[r][1], interval, hidden, &win, &ctx)) return 2;
        if (strcmp(mode, "old") == 0)
        {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);          // rndClose: the X connection closed, the context leaked
        }
        else
        {
            SDL_GL_MakeCurrent(win, NULL);
            SDL_GL_DeleteContext(ctx);
            SDL_DestroyWindow(win);                     // the video subsystem (the X connection) kept
        }
    }
    SDL_Quit();
    printf("done mode %s xerrs %d\n", mode, xerrs);
    return 0;
}
