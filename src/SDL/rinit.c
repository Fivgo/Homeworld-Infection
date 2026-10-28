/*=============================================================================
    Name    : rinit.c
    Purpose : rGL / OpenGL enumeration initialization routines

    Created 1/5/1999 by khent
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/


/*#define WIN32_LEAN_AND_MEAN*/
#define STRICT

#include "rinit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "Debug.h"
#include "StringSupport.h"
#include "Types.h"

#ifdef _WIN32
    #include <windows.h>
#endif

extern unsigned int mainSoftwareDirectDraw;
extern int MAIN_WindowWidth, MAIN_WindowHeight;        // HWI-FEATURE(resolution-presets): the start's request (main.c)
extern unsigned int mainOutputCRC;

extern unsigned int* devTable;
extern int devTableLength;

static rdevice* rDeviceList;
static int nDevices;

#if 0    /* Direct3D stuff...not used. */
char* gDescription = NULL;
char* gDriverName = NULL;

typedef HRESULT(WINAPI * DIRECTDRAWCREATE)(GUID*, LPDIRECTDRAW*, IUnknown*);
typedef HRESULT(WINAPI * DIRECTINPUTCREATE)(HINSTANCE, DWORD, LPDIRECTINPUT*, IUnknown*);
#endif

extern unsigned int sstHardwareExists(int*);
extern unsigned int glCapNT(void);

static void* rinMemAlloc(int size)
{
    unsigned char* block;

    block = (unsigned char*)malloc(size);
    memset(block, 0, size);

    return (void*)block;
}

static void rinMemFree(void* memblock)
{
    free(memblock);
}

extern unsigned int crc32Compute(void*, unsigned int);

/*-----------------------------------------------------------------------------
    Name        : rinDeviceCRC
    Description : generate a CRC of the detected devices for detection of
                  changing hardware (light assurance that previous video
                  mode will still be working correctly)
    Inputs      :
    Outputs     :
    Return      : crc32
----------------------------------------------------------------------------*/
unsigned int rinDeviceCRC(void)
{
    rdevice* cdev;
    rdevice* devlist;
    rdevice* pdevlist;
    unsigned int crc;

    if (nDevices == 0 || rDeviceList == NULL)
    {
        return 0;
    }

    devlist = (rdevice*)rinMemAlloc(nDevices * sizeof(rdevice));
    pdevlist = devlist;

    cdev = rDeviceList;
    do
    {
        memcpy(pdevlist, cdev, sizeof(rdevice));
        pdevlist->modes = NULL;
        pdevlist->next = NULL;
        pdevlist++;
        cdev = cdev->next;
    } while (cdev != NULL);

    crc = crc32Compute((void*)devlist, nDevices * sizeof(rdevice));

    rinMemFree(devlist);

    return crc;
}

/*-----------------------------------------------------------------------------
    Name        : rinAddMode
    Description : add a display mode to a video driver
    Inputs      : dev - the device to add the mode to
                  width, height - dimensions
                  depth - bitdepth
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rinAddMode(rdevice* dev, int width, int height, int depth)
{
    rmode* mode;
    rmode* cmode;

    mode = (rmode*)rinMemAlloc(sizeof(rmode));
    mode->width  = width;
    mode->height = height;
    mode->depth  = depth;

    if (dev->modes == NULL)
    {
        //no modes yet
        dev->modes = mode;
        mode->next = NULL;
    }
    else
    {
        //add to tail
        cmode = dev->modes;
        while (cmode->next != NULL)
        {
            cmode = cmode->next;
        }
        cmode->next = mode;
        mode->next = NULL;
    }
}

/*-----------------------------------------------------------------------------
    HWI-FEATURE(resolution-presets) (M3-R4 window-modes): the user asked for "typical supported resolutions for 16:9,
    16:10, and 4:3 aspect ratios". GoK's list was the integer divisions of display 0's mode (at 3840x2160: 3840x2160,
    1920x1080, 1280x720, 960x540). Now: these presets, each listed when it fits a display (both sides), plus every
    display's own mode, in ascending order; the Video screen adds the window's own size (Options.c opResListSet).
    cataclysm.exe listed the DirectDraw driver's modes (rinEnumeratePrimary 0x0056c8b0).
-----------------------------------------------------------------------------*/
static const struct { int width, height; } rinPresets[] =
{
    {  640,  480 }, {  800,  600 }, { 1024,  768 }, { 1152,  864 }, { 1280,  720 }, { 1280,  800 }, { 1280,  960 },
    { 1280, 1024 }, { 1366,  768 }, { 1400, 1050 }, { 1440,  900 }, { 1600,  900 }, { 1600, 1200 }, { 1680, 1050 },
    { 1920, 1080 }, { 1920, 1200 }, { 2048, 1536 }, { 2560, 1440 }, { 2560, 1600 }, { 3840, 2160 },
};

int rinPresetCount(void)
{
    return (int)(sizeof(rinPresets) / sizeof(rinPresets[0]));
}

void rinPreset(int index, int* width, int* height)
{
    *width = rinPresets[index].width;
    *height = rinPresets[index].height;
}

// a mode inserted in ascending (width, height) order unless the list has it; 1 when added
int rinAddModeIfMissing(rdevice* dev, int width, int height, int depth)
{
    rmode* mode;
    rmode** link;

    if (width <= 0 || height <= 0)
    {
        return 0;
    }
    for (link = &dev->modes; *link != NULL; link = &(*link)->next)
    {
        if ((*link)->width == width && (*link)->height == height && (*link)->depth == depth)
        {
            return 0;
        }
        if ((*link)->width > width || ((*link)->width == width && (*link)->height > height))
        {
            break;
        }
    }
    mode = (rmode*)rinMemAlloc(sizeof(rmode));
    mode->width  = width;
    mode->height = height;
    mode->depth  = depth;
    mode->next = *link;
    *link = mode;
    return 1;
}

// the aspect's name: one of the four families within 1 % (1366x768 is 16:9 to 0.05 %), else the reduced ratio
const char* rinAspectName(int width, int height)
{
    static const struct { int w, h; const char* name; } aspects[] =
    {
        { 4, 3, "4:3" }, { 16, 10, "16:10" }, { 16, 9, "16:9" }, { 5, 4, "5:4" },
    };
    static char name[24];
    int i, a, b, t;

    if (width <= 0 || height <= 0)
    {
        return "?";
    }
    for (i = 0; i < (int)(sizeof(aspects) / sizeof(aspects[0])); i++)
    {
        if (abs(width * aspects[i].h - height * aspects[i].w) * 100 < height * aspects[i].w)
        {
            return aspects[i].name;
        }
    }
    a = width; b = height;
    while (b != 0)
    {
        t = a % b; a = b; b = t;
    }
    snprintf(name, sizeof(name), "%d:%d", width / a, height / a);
    return name;
}

/*-----------------------------------------------------------------------------
    Name        : rinModeAccepted
    Description : decides whether a given mode is supported
    Inputs      : dev - the device in question
                  width, height, depth - display mode characteristics
    Outputs     :
    Return      : true or false
----------------------------------------------------------------------------*/
static bool32 rinModeAccepted(rdevice* dev, int width, int height, int depth)
{
    int index;

    //accept 16, 24, or 32 bit mode only
    if (depth != 16 && depth != 24 && depth != 32)
    {
        return FALSE;
    }

    return TRUE;
}

/*-----------------------------------------------------------------------------
    Name        : rinSortModes
    Description : sorts a device's display modes the way I like them
                  (one bitdepth at a time), and remove duplicates
    Inputs      : dev - the device whose modes are to be sorted
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rinSortModes(rdevice* dev)
{
    rmode* cmode;
    rmode* freeMode;
    rdevice dummy;
    int depths[] = { 32, 24, 16, 0 };
    int depth;

    if (dev->modes == NULL)
    {
        return;
    }

    memset(&dummy, 0, sizeof(rdevice));

    for (depth = 0; depths[depth] != 0; depth++)
    {
        cmode = dev->modes;
        do
        {
            if (cmode->depth == depths[depth])
            {
                if (rinModeAccepted(dev, cmode->width, cmode->height, cmode->depth) &&
                    (cmode->next==NULL || cmode->next->width!=cmode->width ||
                     cmode->next->height!=cmode->height || cmode->next->depth!=cmode->depth))
                {
                    rinAddMode(&dummy, cmode->width, cmode->height, cmode->depth);
                }
            }
            cmode = cmode->next;
        } while (cmode != NULL);
    }

    //free modes on dev
    cmode = dev->modes;
    while (cmode != NULL)
    {
        freeMode = cmode;    //save to free
        cmode = cmode->next; //next
        rinMemFree(freeMode);   //free
    }

    //attach new modes
    dev->modes = dummy.modes;
}

/*-----------------------------------------------------------------------------
    Name        : rinCopyModes
    Description : copy one driver's display modes to another
    Inputs      : devOut - output device
                  devIn - input device
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rinCopyModes(rdevice* devOut, rdevice* devIn)
{
    rmode* cmode;

    cmode = devIn->modes;
    if (cmode == NULL)
    {
        devOut->modes = NULL;
    }
    else
    {
        do
        {
            rinAddMode(devOut, cmode->width, cmode->height, cmode->depth);
            cmode = cmode->next;
        } while (cmode != NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : rinCopyModesSelectively
    Description : copy certain display modes from one device to another
    Inputs      : devOut - output device
                  devIn - input device
                  depth - bitdepth of acceptable modes
                  maxWidth - maximum acceptable width
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void rinCopyModesSelectively(rdevice* devOut, rdevice* devIn, int depth, int maxWidth)
{
    rmode* cmode;

    cmode = devIn->modes;
    if (cmode == NULL)
    {
        devOut->modes = NULL;
    }
    else
    {
        do
        {
            if ((cmode->depth == depth) &&
                (cmode->width <= maxWidth))
            {
                rinAddMode(devOut, cmode->width, cmode->height, cmode->depth);
            }
            cmode = cmode->next;
        } while (cmode != NULL);
    }
}

/*-----------------------------------------------------------------------------
    Name        : rinAddDevice
    Description : add a device to the head of the device list
    Inputs      : dev - the device to add
    Outputs     : rDeviceList is modified
    Return      :
----------------------------------------------------------------------------*/
void rinAddDevice(rdevice* dev)
{
    rdevice* cdev;

    if (rDeviceList == NULL)
    {
        rDeviceList = dev;
        dev->next = NULL;
    }
    else
    {
        //add to head
        cdev = rDeviceList;
        dev->next = cdev;
        rDeviceList = dev;
    }
}

/*-----------------------------------------------------------------------------
    Name        : rinGetDeviceList
    Description : return pointer to device list
    Inputs      :
    Outputs     :
    Return      : rDeviceList
----------------------------------------------------------------------------*/
rdevice* rinGetDeviceList(void)
{
    return rDeviceList;
}

/*-----------------------------------------------------------------------------
    Name        : rinFreeDevices
    Description : clear the device list (free memory and such)
    Inputs      :
    Outputs     : rDeviceList and constituents are freed
    Return      :
----------------------------------------------------------------------------*/
int rinFreeDevices(void)
{
    rdevice* dev;
    rdevice* freeDev;
    rmode* mode;
    rmode* freeMode;

    //device loop
    dev = rDeviceList;
    while (dev != NULL)
    {
        //mode loop
        mode = dev->modes;
        while (mode != NULL)
        {
            freeMode = mode;    //save to free
            mode = mode->next;  //next
            rinMemFree(freeMode);  //free
        }

        freeDev = dev;          //save to free
        dev = dev->next;        //next
        rinMemFree(freeDev);       //free
    }

    rDeviceList = NULL;

    //success
    return 1;
}

/*-----------------------------------------------------------------------------
    Name        : rinMaxWidth
    Description : return max width from all modes of all devices
    Inputs      :
    Outputs     :
    Return      : max available width, or 9999 if no devices available
----------------------------------------------------------------------------*/
int rinMaxWidth(void)
{
    rdevice* cdev;
    rmode* cmode;
    int maxWidth;

    if (nDevices == 0)
    {
        return 9999;
    }

    cdev = rDeviceList;
    maxWidth = 0;
    while (cdev != NULL)
    {
        cmode = cdev->modes;
        while (cmode != NULL)
        {
            if (cmode->width > maxWidth)
            {
                maxWidth = cmode->width;
            }
            cmode = cmode->next;
        }
        cdev = cdev->next;
    }

    return (maxWidth == 0) ? 9999 : maxWidth;
}

/*-----------------------------------------------------------------------------
    Name        : rinEnumeratePrimary
    Description : enumerate available display modes on the primary display
    Inputs      : dev - the device whose modes are to be filled
    Outputs     :
    Return      : true or false (could or couldn't enumerate)
----------------------------------------------------------------------------*/
bool32 rinEnumeratePrimary(rdevice* dev)
{
    SDL_DisplayMode mode;
    Uint32 flags;
    int i, j, n, depth = 32, dw[16], dh[16];

    if (!dev)
        return FALSE;

#ifdef _WIN32
    HRESULT hr = SetProcessDPIAware();
#endif

    /* Make sure SDL video is initialized. */
    flags = SDL_WasInit(SDL_INIT_EVERYTHING);
    if (!flags)
    {
        if (SDL_Init(SDL_INIT_VIDEO) == -1)
            return FALSE;
    }
    else if (!(flags & SDL_INIT_VIDEO))
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) == -1)
            return FALSE;
    }

    // HWI-FEATURE(resolution-presets) (M3-R4 window-modes): every display's current mode (the fit puts a request on the
    //      display it fits, uiscale.c uiScaleFitDisplay); GoK listed the integer divisions of display 0's mode only
    n = SDL_GetNumVideoDisplays();
    n = (n > 16) ? 16 : n;
    for (i = 0; i < n; i++)
    {
        dw[i] = dh[i] = 0;
        if (SDL_GetCurrentDisplayMode(i, &mode) == 0)
        {
            dw[i] = mode.w;
            dh[i] = mode.h;
            if (i == 0)
            {
                depth = SDL_BITSPERPIXEL(mode.format);
                printf("Current display mode: %dx%d %d\n", mode.w, mode.h, SDL_BITSPERPIXEL(mode.format));
            }
            else
            {
                printf("Current display mode: %dx%d %d (display %d)\n", mode.w, mode.h, SDL_BITSPERPIXEL(mode.format), i);
            }
        }
    }
    // the presets that fit a display, then each display's own mode (an ordered, duplicate-free list)
    for (i = 0; i < rinPresetCount(); i++)
    {
        for (j = 0; j < n; j++)
        {
            if (rinPresets[i].width <= dw[j] && rinPresets[i].height <= dh[j])
            {
                rinAddModeIfMissing(dev, rinPresets[i].width, rinPresets[i].height, depth);
                break;
            }
        }
    }
    for (j = 0; j < n; j++)
    {
        if (dw[j] >= 640 && dh[j] >= 480)
        {
            rinAddModeIfMissing(dev, dw[j], dh[j], depth);
        }
    }
    // the start's request (the options file's screenWidth / screenHeight or a /WxH switch: main.c InitWindow enumerates
    // after both are read), so the list holds the size in use even when it is no preset and larger than any display
    // (a dragged size from another desktop); Options.c opResListSet adds the current request the same way later
    if (MAIN_WindowWidth >= 640 && MAIN_WindowHeight >= 480)
    {
        rinAddModeIfMissing(dev, MAIN_WindowWidth, MAIN_WindowHeight, depth);
    }

    return (dev->modes != 0);
}

/*-----------------------------------------------------------------------------
    Name        : rinEnumerateDevices
    Description : populate the device list by enumerating available renderers
    Inputs      :
    Outputs     : rDeviceList is filled
    Return      :
----------------------------------------------------------------------------*/
int rinEnumerateDevices(void)
{
    rdevice* dev;
    rdevice* gldev;
    rdevice  primaryDev;
    bool32 primaryVal;
    
#ifndef _MACOSX_FIX_GL
    int maxWidth;
#endif

    //add OpenGL device
    dev = (rdevice*)rinMemAlloc(sizeof(rdevice));
    dev->type = RIN_TYPE_OPENGL;
    dev->data[0] = '\0';
    strncpy(dev->name, "OpenGL", 63);
    
    dev->modes = NULL;
    memset(&primaryDev, 0, sizeof(rdevice));
    primaryVal = rinEnumeratePrimary(&primaryDev);
    if (primaryVal)
    {
        rinCopyModes(dev, &primaryDev);
    }
    rinSortModes(dev);
    gldev = dev;
    nDevices++;

    rinAddDevice(gldev);

    //success
    return 1;
}
