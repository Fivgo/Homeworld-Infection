// =============================================================================
//  screenshot.c
//  - take screenshots via OpenGL
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created 7/15/1998 by khent
// =============================================================================

#include <SDL2/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"        

#include "screenshot.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/mman.h>
#endif

#include "Debug.h"
#include "glinc.h"
#include "File.h"
#include "main.h"
#include "CommandWrap.h"      // CAT: clCommandMessage (the screenshot message)

#ifdef _WIN32
	#define sleep(x) _sleep((x) * 1000)
#else
    #include <unistd.h>
#endif

static void _ssAppendScreenshotFilename(char* savePath);


// =============================================================================


// CAT: ssSaveScreenshot 0x0057ef80 names a shot "shota%03d.jpg", the first of 000..199 not there yet, else
//      "shot200.jpg" (GoK: "shot_<date>_<time>_<zone>.jpg"). HWI: in the visible user-files folder's Screenshots/
//      (HWI-FEATURE(user-files): Documents/Homeworld Infection/Screenshots; GoK: the settings path's ScreenShots/;
//      the exe: %HW_Data%\screenshots\ or screenshots\ in the current directory)
static void _ssAppendScreenshotFilename(char* savePath)
{
    FILE        *imageFile;
    char         imagePath[PATH_MAX + 1],
                 imageName[32];
    sdword       index;

    for (index = 0; index < 200; index++)
    {
        sprintf(imageName, "shota%03d.jpg", (int)index);
        snprintf(imagePath, sizeof(imagePath), "%s%s", savePath, imageName);
        if ((imageFile = fopen(imagePath, "rt")) == NULL)
        {
            break;                                          // not there yet: this name
        }
        fclose(imageFile);
    }
    if (index == 200)
    {
        strcpy(imageName, "shot200.jpg");
    }
    strcat(savePath, imageName);
}


static void _ssSaveScreenshot(ubyte* buf, const char* targetFilename)
{
    static char fname[PATH_MAX + 64];               // HWI-FEATURE(user-files)
    unsigned char *pTempLine;
    long Top, Bot, i, Size;

    snprintf(fname, PATH_MAX, "%sScreenshots/", fileUserFilesPath);   // HWI-FEATURE(user-files): the visible folder
    if (!fileMakeDirectory(fname))
        return;

    if (!targetFilename)
    {
        _ssAppendScreenshotFilename(fname);
    } else {
        strcat(fname, targetFilename);
    }

    fileNameReplaceSlashesInPlace(fname);

#if SS_VERBOSE_LEVEL >= 1
    dbgMessagef("Saving %dx%d screenshot to '%s'.", MAIN_WindowWidthActual, MAIN_WindowHeightActual, fname);
#endif

    // Flip buffer contents vertically due to OpenGL's origin being in the lower-left corner
    Size = MAIN_WindowWidthActual*3;
    pTempLine = (unsigned char *)malloc(Size);

    for (i = 0; i < (MAIN_WindowHeightActual / 2); i++)
    {
        Top = i;
        Bot = (MAIN_WindowHeightActual - 1) - i;

        memcpy(pTempLine, buf + (Size * Top), Size);
        memcpy(buf + (Size * Top), buf + (Size * Bot), Size);
        memcpy(buf + (Size * Bot), pTempLine, Size);
    }
    free(pTempLine);

    stbi_write_jpg(fname, MAIN_WindowWidthActual, MAIN_WindowHeightActual, 3, buf, 100);

    if (!targetFilename)
    {                                                       // CAT: the exe's message (ssSaveScreenshot 0x0057ef80;
        char message[PATH_MAX + 32];                        //      GoK's named debug shots stay silent)

        snprintf(message, sizeof(message), "Screenshot Saved: %s", fname);
        clCommandMessage(message, 0xffffffff);
    }
}


// CAT: Cataclysm's ssSaveScreenshot 0x0057ef80 (called by rndRenderTask with the pixels read back) is this file's
//      _ssSaveScreenshot with the exe's file names and its "Screenshot Saved: %s" message (M2-R6 startup-keys); GoK's
//      read-back (glReadPixels) and JPEG writer (stb, decision 28's backend) stay.
void ssTakeScreenshot(const char* filename)
{
    ubyte* screenshot_buffer =
#ifdef _WIN32
        (void *)VirtualAlloc(NULL, 3 * MAIN_WindowWidthActual * MAIN_WindowHeightActual,  // 3 = RGB
            MEM_COMMIT, PAGE_READWRITE);
#else
        mmap(NULL, 3 * MAIN_WindowWidthActual * MAIN_WindowHeightActual,
            PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

    if (screenshot_buffer != NULL)
    {
        int result = 0;
        
        glReadPixels(0, 0, MAIN_WindowWidthActual, MAIN_WindowHeightActual,
            GL_RGB, GL_UNSIGNED_BYTE, screenshot_buffer);
            
        _ssSaveScreenshot(screenshot_buffer, filename);

#ifdef _WIN32
        result = VirtualFree(screenshot_buffer, 0, MEM_RELEASE);
        dbgAssertOrIgnore(result);
#else
        result = munmap(screenshot_buffer, 3*MAIN_WindowWidthActual*MAIN_WindowHeightActual);
        dbgAssertOrIgnore(result != -1);
#endif
    }
}
