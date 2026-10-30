/*=============================================================================
    Name    : avi.c
    Purpose : routines for playing AVI files

    Created 1/9/1999 by jdorie, khent, jjoire
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/
/* TC 2003-10-01:
 * Ever so silently, we replace AVI functions with dummy functions when
 * compiling on platforms other than Windows.  Shh...don't tell the game what
 * we're doing...
 */

#include "Animatic.h"
#include "avi.h"
#include "Debug.h"
#include "File.h"
#include "glinc.h"
#include "NIS.h"
#include "Subtitle.h"
#include "Tutor.h"
#include "utility.h"
#include "Universe.h"
#include "render.h"
#include "SoundEvent.h"
#include "main.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

//nova: this allows MSVC to compile this file
#ifdef _WIN32
 	#define sleep(x) _sleep((x) * 1000)
#endif

#ifdef HW_ENABLE_MOVIES_WIN32
    #include <windows.h>
    #include <Vfw.h>
    #include "wave.h"
#endif

#ifdef HW_ENABLE_MOVIES
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #define ALIGN 32
#endif


/*=============================================================================
    Switches
=============================================================================*/


extern bool32 fullScreen;
extern void* ghMainWindow;
extern int MAIN_WindowWidth;
extern int MAIN_WindowHeight;
extern bool32 systemActive;

bool32 g_bMoreFrames;

int aviDonePlaying = 1;
int aviIsPlaying = 0;
int aviHasAudio = 0;

// HWI-FEATURE(intro-movies) (M3-R4 intro-movies): the intro logos play at start-up as in cataclysm.exe (utyGameSystemsInit
//      0x005abe70 plays them whenever enableAVI, i.e. without /disableAVI). GoK's options row PlayIntros (no exe
//      counterpart) defaulted to 0; it is now 1, and a Cataclysm.cfg row "PlayIntros 0" still turns them off.
udword aviPlayIntros = 1;

#ifdef HW_ENABLE_MOVIES
AVFormatContext *pFormatCtx    = NULL;
AVCodecContext  *pCodecCtx     = NULL;
AVStream        *streamPointer = NULL;
AVCodec const   *pCodec        = NULL;
AVFrame         *pFrame        = NULL;
AVDictionary    **opts         = NULL;
AVCodecParameters *pCodecpar   = NULL;
AVCodecParserContext *parser   = NULL;
#endif

int aviMovieExpandFactor = 1;

// CAT (M2-R6 nis-fow-camera): the movie's frame rate. The exe's animatic decode callback (0x004217d0) converts the
//      frame number to seconds by Bink's frame rate (0x00a70f1c); Cataclysm's animatics are 30 frames a second (HW1's
//      were 15, which GoK's player assumed: the animatics played at half speed). aviStart reads it from the stream.
real32 aviFrameRate = 15.0f;

#ifdef HW_ENABLE_MOVIES_WIN32	/* Disable AVI code outside of Windows. */

PAVISTREAM    g_VidStream;      //the AVI video stream
AVISTREAMINFO g_VidStreamInfo;  //info about the AVI stream

PAVISTREAM    g_AudStream;      //the AVI audio stream
AVISTREAMINFO g_AudStreamInfo;  //info about the AVI stream

long          g_dwCurrFrame;    //current frame of the AVI
long          g_dwCurrSample;   //current sample of the AVI
PGETFRAME     g_pFrame;         //an object to hold info about the video frame when we get it

double g_FramesPerSec;
double g_SamplesPerSec;

HBITMAP g_hBitmap = 0;
WORD*   g_pBitmap = 0;

MMRESULT g_timerHandle = NULL;

int aviVerifyResult(HRESULT Result)
{
    return (Result == 0 ? 1 : 0);
}


BOOL aviGetNextFrame(BITMAPINFO** ppbmi)
{
    *ppbmi = (LPBITMAPINFO)AVIStreamGetFrame(g_pFrame, g_dwCurrFrame);
    g_dwCurrFrame++;

    //any more frames left?
    return !(g_dwCurrFrame >= g_VidStreamInfo.dwLength);
}

void aviResetStream(void)
{
    g_dwCurrFrame  = 0;
    g_dwCurrSample = 0;
}


void aviShowFrame(BITMAPINFO* pMap)
{
    HDC hdc, hdcTemp;
    int XbmSize, YbmSize;
    int xOffset, yOffset;

    XbmSize = pMap->bmiHeader.biWidth;
    YbmSize = pMap->bmiHeader.biHeight;

    hdc = GetDC(ghMainWindow);
    g_hBitmap = CreateDIBSection(hdc, pMap, DIB_RGB_COLORS, (VOID**)&g_pBitmap, NULL, NULL);
    SetDIBits(hdc, g_hBitmap, 0, YbmSize, (BYTE*)(&pMap->bmiColors), pMap, DIB_RGB_COLORS);

    hdcTemp = CreateCompatibleDC(hdc);
    SelectObject(hdcTemp, g_hBitmap);

    if (!fullScreen &&
        (XbmSize != MAIN_WindowWidth || YbmSize != MAIN_WindowHeight))
    {
        xOffset = (MAIN_WindowWidth  - XbmSize) / 2;
        yOffset = (MAIN_WindowHeight - YbmSize) / 2;
    }
    else
    {
        xOffset = 0;
        yOffset = 0;
    }
    BitBlt(hdc, xOffset, yOffset, XbmSize, YbmSize, hdcTemp, 0, 0, SRCCOPY);

    DeleteDC(hdcTemp);
    ReleaseDC(ghMainWindow, hdc);

    DeleteObject(g_hBitmap);
}

BITMAPINFO* g_pbmi = NULL;

void CALLBACK aviTimeProc(UINT uid, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
{
	if (g_bMoreFrames)
	{
		g_bMoreFrames = aviGetNextFrame(&g_pbmi);
	}
	else
	{
		aviDonePlaying = TRUE;
	}
}

#endif	/* _WIN32 */

// HWI-FIX(release-1.0): declared outside HW_ENABLE_MOVIES, because aviShotTake (outside that block) reads it; a
//      -Dmovies=false build (tools/release_win64.sh) did not compile
static int aviRectLast[4] = { 0, 0, 0, 0 };         // the last frame's picture placement (physical x, y, w, h)

#ifdef HW_ENABLE_MOVIES
/*-----------------------------------------------------------------------------
    HWI-FEATURE(intro-movies) (M3-R4 intro-movies). The user: "renable the starting cutscenes for sierra and barking dog
    and maintain their original aspect ratio instead of stretching them out".

    The picture keeps its own aspect: the part of the 640x480 frame that holds the picture is fitted whole into the
    window and centred; the rest of the window is black. The picture (every frame of the 20 movies measured):
      the intro logos (sierra.bik, bdogintro.bik): the whole frame, 640x480 (4:3: their picture reaches every edge)
      the animatics (Animatic01..18.bik): rows 48..431 of the frame, 640x384 (5:3), black above and below
    so at 3840x2160 a logo is drawn 2880x2160 with bars left and right, and an animatic's picture 3600x2160.
    cataclysm.exe blits the 640x480 frame 1:1 in the middle of a window, or switches a fullscreen display to 640x480
    (binkDisplayWin32 0x00551350 and hwSetRes), which a widescreen monitor stretches. GoK assumed HW1's 1.85:1 picture
    for every movie: at 16:9 it cut a quarter of a logo's height and about 6% off the top and bottom of an animatic.

    The frame's texture is made for each movie and deleted at its end (aviTexDelete), so a movie after a resolution
    switch (a new GL context: render.c rndWindowDestroy) never binds a texture of the old context (GoK kept one texture
    name for the whole run); its filter is set after it is bound (GoK set it on whatever texture was bound before); it
    is the frame's own size (GoK's power-of-two texture blended its black padding into the right and top edges). The
    GL state the draw changes is saved and restored around it (glPushAttrib), so the renderer's cached state
    (rndTextureEnvironment, rndTextureEnabled) stays true for the subtitles and the game after the movie (GoK left
    GL_REPLACE and GL_TEXTURE_2D set behind the cache).
-----------------------------------------------------------------------------*/
extern int MAIN_WindowWidthActual;
extern int MAIN_WindowHeightActual;

static GLuint aviTex = 0;                           // this movie's frame texture (0: none)
static int aviTexW = 0, aviTexH = 0;
bool32 aviIntroMovie = FALSE;                       // TRUE: an intro logo (the whole frame is the picture; no script)

static void aviTexDelete(void)
{
    if (aviTex != 0)
    {
        glDeleteTextures(1, &aviTex);
        aviTex = 0;
    }
    aviTexW = aviTexH = 0;
}

static void aviTexUpload(int w, int h, int stride, const ubyte *data)
{
    if (aviTex == 0 || aviTexW != w || aviTexH != h)
    {
        aviTexDelete();
        glGenTextures(1, &aviTex);
        glBindTexture(GL_TEXTURE_2D, aviTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        aviTexW = w;
        aviTexH = h;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, aviTex);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 3);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
}

/*-----------------------------------------------------------------------------
    Name        : aviPictureRect
    Description : HWI-FEATURE(intro-movies): where a w x h frame is drawn in a physW x physH window: scaled so the
                  picture (the whole frame, or the animatics' 5:3 band) fits whole, centred. The frame's black bands
                  may fall outside the window (an animatic at 16:9).
    Outputs     : rect - x, y (from the bottom-left, as GL counts), width, height in physical pixels
----------------------------------------------------------------------------*/
void aviPictureRect(int w, int h, int physW, int physH, int rect[4])
{
    real64 picW = (real64)w;
    real64 picH = aviIntroMovie ? (real64)h : (real64)w * 3.0 / 5.0;
    real64 scale;

    if (picH > (real64)h)
    {
        picH = (real64)h;
    }
    scale = (real64)physW / picW;
    if ((real64)physH / picH < scale)
    {
        scale = (real64)physH / picH;
    }
    rect[2] = (int)floor((real64)w * scale + 0.5);
    rect[3] = (int)floor((real64)h * scale + 0.5);
    rect[0] = (physW - rect[2]) / 2;
    rect[1] = (physH - rect[3]) / 2;
}

void aviDisplayFrame(AVFrame *pFrameRGB, int w, int h)
{
    int physW = MAIN_WindowWidthActual, physH = MAIN_WindowHeightActual;
    int rect[4];
    real32 sx, sy, x0, y0, x1, y1;

    if (physW <= 0 || physH <= 0)
    {
        physW = MAIN_WindowWidth;
        physH = MAIN_WindowHeight;
    }
    aviPictureRect(w, h, physW, physH, rect);
    if (memcmp(rect, aviRectLast, sizeof(rect)) != 0)
    {
        memcpy(aviRectLast, rect, sizeof(rect));
        dbgMessagef("movie: drawn at %d,%d size %dx%d in the window's %dx%d (logical %dx%d), picture %s",
                    rect[0], rect[1], rect[2], rect[3], physW, physH, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
                    aviIntroMovie ? "the whole frame (4:3)" : "the 5:3 band (rows 48..431)");
    }
    sx = (real32)MAIN_WindowWidth / (real32)physW;          // physical pixels to the ortho's logical units
    sy = (real32)MAIN_WindowHeight / (real32)physH;
    x0 = (real32)rect[0] * sx;
    y0 = (real32)rect[1] * sy;
    x1 = (real32)(rect[0] + rect[2]) * sx;
    y1 = (real32)(rect[1] + rect[3]) * sy;

    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_VIEWPORT_BIT | GL_COLOR_BUFFER_BIT | GL_CURRENT_BIT |
                 GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT | GL_SCISSOR_BIT);
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    animAviSetup(TRUE);
    glViewport(0, 0, physW, physH);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);                           // the bars

    aviTexUpload(w, h, pFrameRGB->linesize[0], pFrameRGB->data[0]);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glBegin(GL_TRIANGLE_STRIP);                             // the frame's first row at the top
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(x0, y0);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(x1, y0);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(x0, y1);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(x1, y1);
    glEnd();

    animAviSetup(FALSE);
    glPopClientAttrib();
    glPopAttrib();
}

#endif

void aviSubUpdate(void) {

    int index;

    for (index = 0; index < SUB_NumberRegions; index++)
    {
        if ( index == STR_LetterboxBar)
        {                                           //if this is the subtitle region
            tutDrawTextPointers(&subRegion[STR_LetterboxBar].rect);//draw any active pointers there may be
        }
        if (subRegion[index].bEnabled && subRegion[index].cardIndex > 0)
        {
            if (index == STR_NIS)
            {
                subTimeElapsed = &thisNisPlaying->timeElapsed;
            }
            else
            {
                subTimeElapsed = &universe.totaltimeelapsed;
            }

            // disable dropshadow for movie subtitles since they are somehow buggy
            // and it seems like they were supposed to be drawn differently
            // TODO: figure out why it is buggy
            
            subcard *card;
            int index2;
            for (index2 = 0, card = subRegion[index].card; index2 < subRegion[index].cardIndex; index2++, card++)
            {
                if (card->bDropShadow)
                {
                    card->bDropShadow = FALSE;
                }
            }

            subTitlesDraw(&subRegion[index]);
        }
    }

}

/*-----------------------------------------------------------------------------
    HWI-FEATURE(intro-movies) (M3-R4 intro-movies): the events while a movie plays. A key press (not a key's repeat)
    ends the movie (GoK's rule; cataclysm.exe ends it on Escape's key-up: WindowProc 0x00561f10 calls binkStop). A closed
    window ends it and its SDL_QUIT is put back for the main loop, which then quits as usual (HandleEvent's quit runs
    WindowsCleanup, not a thing to run inside utyGameSystemsInit). A window event goes to HandleEvent: a dragged size
    applies at once and its relayout waits for the movie's end (uiscale.c, aviIsPlaying). GoK read one event a frame,
    acted on a key only and dropped the rest (a closed window was ignored until the movie ended); the rest is still
    dropped.
-----------------------------------------------------------------------------*/
#define AVI_PLAYING     0
#define AVI_ENDED       1
#define AVI_KEY         2
#define AVI_QUIT        3
#define AVI_CAP         4
static const char *aviWhyName[] = { "playing", "the end", "a key", "the window closed", "the test's frame cap" };

bool32 aviQuitAsked = FALSE;                        // HWI-FEATURE(intro-movies): the window was closed during a movie
sdword aviFrameCap = 0;                             // HWI-TEST (/catMovieTest): a movie ends after this many frames
static sdword aviShotFrames[8];                     // HWI-TEST (/catMovieShot): the frames read back
static sdword aviShotCount = 0;
static int aviVideoStream = -1;
static sdword aviFrameTotal = 0;                    // the stream's frame count (the log)

static int aviEvents(void)
{
    SDL_Event e;
    int why = AVI_PLAYING;

    while (why != AVI_QUIT && SDL_PollEvent(&e))
    {
        switch (e.type)
        {
            case SDL_KEYDOWN:
                if (!e.key.repeat)
                {
                    why = AVI_KEY;
                }
                break;
            case SDL_QUIT:
                why = AVI_QUIT;
                break;
            case SDL_WINDOWEVENT:
                HandleEvent(&e);
                break;
            default:
                break;
        }
    }
    if (why == AVI_QUIT)
    {
        SDL_Event q;

        aviQuitAsked = TRUE;
        memset(&q, 0, sizeof(q));
        q.type = SDL_QUIT;
        SDL_PushEvent(&q);
    }
    return why;
}

// HWI-TEST (M3-R4 intro-movies): /catMovieShot <frame>[,<frame>...] (up to 8): those frames of every movie are read back
//      after the draw, before the swap, into <settings>/movie-<name>-<frame>.ppm (the window's physical size), one
//      "movieshot:" line each. Read only.
void aviMovieShotSet(char *spec)
{
    char *p = spec;

    aviShotCount = 0;
    while (p != NULL && *p != '\0' && aviShotCount < (sdword)(sizeof(aviShotFrames) / sizeof(aviShotFrames[0])))
    {
        aviShotFrames[aviShotCount++] = (sdword)strtol(p, &p, 10);
        while (*p == ',' || *p == ' ')
        {
            p++;
        }
    }
}

// HWI-TEST (M3-R4 intro-movies): /catMovieEventAt <frame>:quit|<W>x<H>: once, after that frame of a movie is shown, the
//      window's close request (an SDL_QUIT in the engine's own queue, as HandleEvent itself posts one) or a new window
//      size (SDL_SetWindowSize, as the Video screen's mode switch sets it). No keyboard or mouse event is made.
static sdword aviEventFrame = -1;
static sdword aviEventW = 0, aviEventH = 0;         // 0 x 0: the close request
void aviMovieEventSet(char *spec)
{
    int frame = -1, w = 0, h = 0;
    char what[16] = "";

    if (sscanf(spec, "%d:%15s", &frame, what) == 2 && frame >= 0)
    {
        if (strcmp(what, "quit") == 0 || sscanf(what, "%dx%d", &w, &h) == 2)
        {
            aviEventFrame = frame;
            aviEventW = w;
            aviEventH = h;
        }
    }
}

static void aviTestEvent(const char *name, sdword frame)
{
    extern SDL_Window *sdlwindow;

    if (frame != aviEventFrame)
    {
        return;
    }
    aviEventFrame = -1;                             // once
    if (aviEventW > 0 && aviEventH > 0 && sdlwindow != NULL)
    {
        dbgMessagef("movietest: %s frame %d: the window set to %dx%d", name, (int)frame, (int)aviEventW, (int)aviEventH);
        SDL_SetWindowSize(sdlwindow, aviEventW, aviEventH);
    }
    else
    {
        SDL_Event q;

        dbgMessagef("movietest: %s frame %d: the window's close request", name, (int)frame);
        memset(&q, 0, sizeof(q));
        q.type = SDL_QUIT;
        SDL_PushEvent(&q);
    }
}

static void aviShotTake(const char *name, sdword frame)
{
    sdword i;
    int w = MAIN_WindowWidthActual, h = MAIN_WindowHeightActual, row;
    ubyte *pixels;
    char file[1024];
    FILE *f;

    for (i = 0; i < aviShotCount && aviShotFrames[i] != frame; i++)
    {
    }
    if (i == aviShotCount || w <= 0 || h <= 0)
    {
        return;
    }
    pixels = malloc((size_t)w * (size_t)h * 3);
    if (pixels == NULL)
    {
        return;
    }
    snprintf(file, sizeof(file), "movie-%s-%d.ppm", name, (int)frame);
    snprintf(file, sizeof(file), "%s", filePathPrepend(file, FF_UserSettingsPath));
    glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glPopClientAttrib();
    if ((f = fopen(file, "wb")) == NULL)
    {
        dbgMessagef("movieshot: FAIL cannot write %s", file);
        free(pixels);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (row = h - 1; row >= 0; row--)
    {
        fwrite(pixels + (size_t)row * (size_t)w * 3, 1, (size_t)w * 3, f);
    }
    fclose(f);
    free(pixels);
    dbgMessagef("movieshot: movie=%s frame=%d size=%dx%d rect=%d,%d,%d,%d file=%s", name, (int)frame, w, h,
                aviRectLast[0], aviRectLast[1], aviRectLast[2], aviRectLast[3], file);
}

/*-----------------------------------------------------------------------------
    Name        : aviPlayLoop
    Description : decodes and draws the open movie at its own frame rate. HWI-FEATURE(intro-movies): with a music
                  track (an intro logo) the track plays from the first frame and stops at the end (binkPlay 0x00551840:
                  soundEventPlayMusic before its loop, soundEventStopMusic(0) after it); holdMs keeps the last frame
                  on screen that long after the music stops (the exe's Sleep(2000) after sierra.bik; redrawn, so the
                  window answers and a resize is drawn; a key ends the wait).
    Return      : why it ended (AVI_ENDED, AVI_KEY, AVI_QUIT, AVI_CAP)
----------------------------------------------------------------------------*/
static int aviPlayLoop(const char *name, sdword track, udword holdMs)
{
    int why = AVI_PLAYING;

#ifdef HW_ENABLE_MOVIES

    sdword frame = 0;
    int numBytes;
    Uint32 start_time, hold_start;
    ubyte *buffer;
    AVFrame PictureRGB;
    AVFrame *pPictureRGB = &PictureRGB;
    AVPacket *packet;
    static struct SwsContext *img_convert_ctx = NULL;

    memset(&PictureRGB, 0, sizeof(PictureRGB));
    memset(aviRectLast, 0, sizeof(aviRectLast));
    numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, ALIGN);
    buffer = av_malloc(numBytes);
    packet = av_packet_alloc();
    if (buffer == NULL || packet == NULL)
    {
        av_free(buffer);
        av_packet_free(&packet);
        return AVI_ENDED;
    }
    av_image_fill_arrays(pPictureRGB->data, pPictureRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width,
                         pCodecCtx->height, ALIGN);
    img_convert_ctx = sws_getCachedContext(img_convert_ctx, pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                           pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC,
                                           NULL, NULL, NULL);

    if (track != -1)
    {
        soundEventPlayMusic(track);
    }
    start_time = SDL_GetTicks();

    while (why == AVI_PLAYING && av_read_frame(pFormatCtx, packet) >= 0)
    {
        if (packet->stream_index == aviVideoStream)
        {
            avcodec_send_packet(pCodecCtx, packet);
        }
        av_packet_unref(packet);

        while (why == AVI_PLAYING && avcodec_receive_frame(pCodecCtx, pFrame) == 0)
        {
            sws_scale(img_convert_ctx, (ubyte const* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
                      pPictureRGB->data, pPictureRGB->linesize);

            if (!aviIntroMovie)
            {
                animAviDecode(frame);                       // an animatic's script (the logos have none)
            }
            soundEventUpdate();
            speechEventUpdate();   //Keep this it works. :)

            aviDisplayFrame(pPictureRGB, pCodecCtx->width, pCodecCtx->height);
            if (!aviIntroMovie)
            {
                aviSubUpdate();
            }
            aviShotTake(name, frame);
            rndFlush();
            aviTestEvent(name, frame);
            frame++;

            why = aviEvents();
            if (why == AVI_PLAYING && aviFrameCap > 0 && frame >= aviFrameCap)
            {
                why = AVI_CAP;
            }

            // CAT: paced at the movie's own frame rate (aviFrameRate; GoK assumed 15 fps)
            while (why == AVI_PLAYING && SDL_GetTicks() < (start_time + (Uint32)((real32)frame * 1000.0f / aviFrameRate)))
                SDL_Delay(1);
        }
    }
    if (why == AVI_PLAYING)
    {
        why = AVI_ENDED;
    }
    av_packet_free(&packet);
    if (track != -1)
    {
        soundEventStopMusic(0.0f);
    }
    dbgMessagef("movie: end file=%s frames=%d of %d in %u ms (%s)", name, (int)frame, (int)aviFrameTotal,
                (unsigned)(SDL_GetTicks() - start_time), aviWhyName[why]);

    if (holdMs > 0 && why != AVI_QUIT && frame > 0)
    {
        int holdWhy = AVI_PLAYING;

        hold_start = SDL_GetTicks();
        while (holdWhy == AVI_PLAYING && SDL_GetTicks() - hold_start < holdMs)
        {
            aviDisplayFrame(pPictureRGB, pCodecCtx->width, pCodecCtx->height);
            rndFlush();
            holdWhy = aviEvents();
            SDL_Delay(15);
        }
        dbgMessagef("movie: held the last frame of %s %u ms of %u (%s)", name, (unsigned)(SDL_GetTicks() - hold_start),
                    (unsigned)holdMs, (holdWhy == AVI_PLAYING) ? "the end" : aviWhyName[holdWhy]);
        if (holdWhy == AVI_QUIT)
        {
            why = AVI_QUIT;
        }
    }

    av_free(buffer);
    aviTexDelete();

#endif //  HW_ENABLE_MOVIES

    return why;
}

int aviStart(char* filename)
{

#ifdef HW_ENABLE_MOVIES

    int i = 0;
    int alignDoubleSet = 1;
    int ffmpegAlign = 1;
    int videoStream = -1;

// Add intelligence to identifying path and OS here  -- TODO

    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0) {
        dbgMessagef("aviStart: Unable to open AVI: %s",filename);
        return FALSE;
    }

#if AVI_VERBOSE_LEVEL >= 2
    av_dump_format(pFormatCtx, 0, filename, 0);
#endif

#if AVI_VERBOSE_LEVEL >= 2
    dbgMessagef("sizeof  AVFormatContext = %d",sizeof(AVFormatContext));
#endif


    avformat_find_stream_info(pFormatCtx, NULL);

    videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);

#if AVI_VERBOSE_LEVEL >= 3
    dbgMessagef("aviStart: Found Video Stream= %d.", i);
#endif

    if(videoStream==-1) {
        dbgMessage("aviStart: No Video Stream found");
        return FALSE;
    }
    aviVideoStream = videoStream;                       // HWI-FEATURE(intro-movies): the loop decodes this stream only
    aviFrameTotal = (sdword)pFormatCtx->streams[videoStream]->nb_frames;
    if (aviFrameTotal <= 0 && pFormatCtx->streams[videoStream]->duration > 0)
    {                                                   // Bink's demuxer gives the length as a duration only
        AVRational fr = pFormatCtx->streams[videoStream]->avg_frame_rate;
        if (fr.num > 0 && fr.den > 0)
        {
            aviFrameTotal = (sdword)llrint((double)pFormatCtx->streams[videoStream]->duration *
                                           av_q2d(pFormatCtx->streams[videoStream]->time_base) * av_q2d(fr));
        }
    }

    // CAT: the stream's own frame rate (see aviFrameRate)
    {
        AVRational rate = pFormatCtx->streams[videoStream]->avg_frame_rate;

        if (rate.num <= 0 || rate.den <= 0)
        {
            rate = pFormatCtx->streams[videoStream]->r_frame_rate;
        }
        aviFrameRate = (rate.num > 0 && rate.den > 0) ? (real32)rate.num / (real32)rate.den : 15.0f;
    }


/*
    pCodecpar = pFormatCtx->streams[0]->codecpar;


    pCodecpar->width=640;   //mpeg4 does not give this in bitstream
    pCodecpar->height=480;  //whatever that means
    pCodec=avcodec_find_decoder(pCodecpar->codec_id);
    parser = av_parser_init(pCodec->id);
    pCodecCtx = avcodec_alloc_context3(pCodec);
    pCodecCtx->width=640;
    pCodecCtx->height=480;
    pCodecCtx->pix_fmt=AV_PIX_FMT_YUV420P;  //manually set the pixel format from ffplay output
*/
    pCodecCtx = avcodec_alloc_context3(pCodec);

    int ret;
    char error[64];
    if( ( ret = avcodec_parameters_to_context( pCodecCtx, pFormatCtx->streams[videoStream]->codecpar ) ) < 0 )
	{
		av_strerror( ret, error, sizeof( error ) );
		dbgMessagef( "idCinematic: Failed to create video codec context from codec parameters with error: %s\n", error );
	}

#if AVI_VERBOSE_LEVEL >= 2
    dbgMessagef("aviStart: Codec required: %d.", pFormatCtx->streams[videoStream]->codecpar->codec_id);
    //dbgMessagef("aviStart: Pix Format: %d.", pFormatCtx->streams[videoStream]->codecpar->pix_fmt);
    //dbgMessagef("Frame Rate: %d/%d", pCodecCtx->frame_rate, pCodecCtx->frame_rate_base);
#endif

// Find the decoder for the video stream

    if(pCodec==NULL) {
        dbgMessage("Unable to find decoder.");
        return FALSE;
    }

#if AVI_VERBOSE_LEVEL >= 2
    dbgMessagef("aviStart: Codec required: %s.", pCodec->name);
#endif

/* this results in corrupted video */
/*    if(pCodec->capabilities & CODEC_CAP_TRUNCATED) {
        dbgMessage("Problem with the codec dealing with truncated frames.");
        pCodecCtx->flags|=CODEC_FLAG_TRUNCATED;
    }*/

// Open codec
    if( ( ret = avcodec_open2(pCodecCtx, pCodec, NULL) ) < 0 )
	{
		av_strerror( ret, error, sizeof( error ) );
		dbgMessagef( "Cannot open video decoder with error: %s\n", error );
		dbgMessage("Unable to open Codec");
        return FALSE;
	}

    pFrame=av_frame_alloc();    //what does this do?
                                //this is the last line in this function....


#endif // HW_ENABLE_MOVIES
    //so good so far
    return TRUE;
}


int aviGetSamples(void* pBuf, long* pNumSamples, long nBufSize)
{
#ifdef HW_ENABLE_MOVIES_WIN32
    HRESULT Res;
    long nSampRead;

	if(aviHasAudio == FALSE) return FALSE;

	if(aviIsPlaying == FALSE)
	{
		memset(pBuf,0,nBufSize);
		return TRUE;
	}

    Res = AVIStreamRead(g_AudStream, g_dwCurrSample, *pNumSamples, pBuf, nBufSize, NULL, &nSampRead);
    if (!aviVerifyResult(Res))
    {
        return FALSE;
    }

    *pNumSamples = nSampRead;
    g_dwCurrSample += nSampRead;

	return TRUE;
#else
    return 0;
#endif
}

void aviFileExit (void){

#ifdef HW_ENABLE_MOVIES
    avformat_close_input(&pFormatCtx);
#endif

}
	
void aviSetScreen(int w, int h){

    int xOfs, yOfs;

    xOfs = (MAIN_WindowWidth  - 640) / 2;
    yOfs = (MAIN_WindowHeight - 480) / 2;

dbgMessagef("aviSetScreen: xOfs=%d yOfs=%d", xOfs, yOfs);

}

int aviStop(void)
{
#ifdef HW_ENABLE_MOVIES
   
    SDL_Delay(500); // Give the audio time to stop

    av_frame_free(&pFrame);                         // HWI-FEATURE(intro-movies): GoK never freed it (one leaked per movie)
    avcodec_free_context(&pCodecCtx); 
    aviFileExit();
#endif
    return 1;
}


/*-----------------------------------------------------------------------------
    Name        : aviPlayMovie
    Description : plays one movie (HWI-FEATURE(intro-movies): GoK's aviPlay with binkPlay's music track and the hold)
    Inputs      : filename - Movies/<name>.bik; track - the music track, or -1; holdMs - see aviPlayLoop
    Return      : why it ended (AVI_ENDED, AVI_KEY, AVI_QUIT, AVI_CAP), or -1 if it could not be opened
----------------------------------------------------------------------------*/
static int aviPlayMovie(char *filename, sdword track, udword holdMs)
{
    char fullname[1024];
    char name[64];
    const char *base;
    char *dot;
    int why;

#if AVI_VERBOSE_LEVEL >= 2
dbgMessage("aviPlay:Entering");
#endif

    strcpy(fullname, filePathPrepend(filename, FF_HomeworldDataPath));

    //try Homeworld\\Data\\Movies first
    if (!aviStart(fullname))
    {
        //try current directory next
        if (!aviStart(filename))
        {
            dbgMessagef("movie: cannot open %s", filename);
            return -1;
        }
    }
    base = strrchr(filename, '/');
    if (base == NULL)
    {
        base = strrchr(filename, '\\');
    }
    base = (base != NULL) ? base + 1 : filename;
    snprintf(name, sizeof(name), "%s", base);
    if ((dot = strrchr(name, '.')) != NULL)
    {
        *dot = '\0';
    }
#ifdef HW_ENABLE_MOVIES
    dbgMessagef("movie: start file=%s %dx%d %.2f fps frames=%d music=%d%s", filename, pCodecCtx->width,
                pCodecCtx->height, (double)aviFrameRate, (int)aviFrameTotal, (int)track,
                aviIntroMovie ? " (an intro logo)" : "");
#endif

    if (track != -1)
    {                                                   // binkPlay 0x00551840's lead-in before a track
        soundEventUpdate();
        speechEventUpdate();
        SDL_Delay(150);
        soundEventUpdate();
        speechEventUpdate();
    }

    g_bMoreFrames  = TRUE;
    aviIsPlaying   = TRUE;
    aviDonePlaying = FALSE;
    why = aviPlayLoop(name, track, holdMs);

    aviStop();
    aviIsPlaying = FALSE;
    aviDonePlaying = TRUE;

    return why;
}

bool32 aviPlay(char* filename)
{
    return aviPlayMovie(filename, -1, 0) >= 0;
}

int aviInit()
{

//#ifdef HW_ENABLE_MOVIES
    //av_register_all(); //can be ignored in ffmpeg 4+
//#endif

    return 1;

}


int aviCleanup()
{
#ifdef HW_ENABLE_MOVIES_WIN32
	//cleanup audio
	if(EndWave() < 0) return FALSE;

    return TRUE;
#else
    return 1;
#endif
}

/*-----------------------------------------------------------------------------
    Name        : aviIntroPlay
    Description : HWI-FEATURE(intro-movies) (M3-R4 intro-movies): the intro logos, as cataclysm.exe's
                  utyGameSystemsInit (0x005abe70) plays them whenever enableAVI (no /disableAVI): binkInit;
                  Movies\\sierra.bik with music track 0x3c (60) and then Sleep(2000); Movies\\bdogintro.bik with track
                  0x3d (61; Music4Cat's 62 tracks end with these two); binkCleanup; mainCleanupAfterVideo. The exe
                  plays them before its renderer starts (Bink blits with GDI); here they need GL, so they play after
                  rndInit, where GoK left its placeholder (GoK played sierra.bik and HW1's relicintro.bik, with no
                  music, and only with its PlayIntros row set, default 0). Off: Cataclysm.cfg "PlayIntros 0",
                  /disableAVI, a build without movie support (the Windows build), and a headless run (the virtual
                  clock, /debugScreenshots: animHeadlessRun, as for an animatic). A closed window ends them.
----------------------------------------------------------------------------*/
void aviIntroPlay(void)
{
    const char *off = NULL;
    int why;
    Uint32 start = SDL_GetTicks();

    if (!enableAVI)
    {
        off = "/disableAVI";
    }
    else if (!aviPlayIntros)
    {
        off = "PlayIntros 0 in Cataclysm.cfg";
    }
    else if (animHeadlessRun())
    {
        off = "a headless run";
    }
#ifndef HW_ENABLE_MOVIES
    if (off == NULL)
    {
        off = "no movie support in this build";
    }
#endif
    if (off != NULL)
    {
        dbgMessagef("intro: off (%s)", off);
        return;
    }

    utilPlayingIntro = TRUE;
    aviIntroMovie = TRUE;
    aviInit();
#ifdef _WIN32
    why = aviPlayMovie("Movies\\sierra.bik", 0x3c, 2000);
    if (!aviQuitAsked)
    {
        why = aviPlayMovie("Movies\\bdogintro.bik", 0x3d, 0);
    }
#else
    why = aviPlayMovie("Movies/sierra.bik", 0x3c, 2000);
    if (!aviQuitAsked)
    {
        why = aviPlayMovie("Movies/bdogintro.bik", 0x3d, 0);
    }
#endif
    aviIntroMovie = FALSE;
    mainCleanupAfterVideo();
    utilPlayingIntro = FALSE;
    dbgMessagef("intro: done in %u ms%s", (unsigned)(SDL_GetTicks() - start),
                aviQuitAsked ? " (the window was closed: the game quits)" : "");
    (void)why;
}
