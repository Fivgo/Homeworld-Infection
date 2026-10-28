/*=============================================================================
    Name    : rinit.h
    Purpose : rGL / OpenGL enumeration initialization routines

    Created 1/5/1999 by khent
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#ifndef ___RINIT_H
#define ___RINIT_H

typedef struct rmode
{
    int width, height;
    int depth;
    struct rmode* next;
} rmode;

#define RIN_TYPE_OPENGL   1

typedef struct rdevice
{
    int type;
    char data[64];
    char name[64];

    struct rmode* modes;

    struct rdevice* next;
} rdevice;

#ifdef __cplusplus
extern "C" {
#endif

int rinEnumerateDevices(void);
int rinFreeDevices(void);
unsigned int rinDirectXVersion(void);
rdevice* rinGetDeviceList(void);

unsigned int rinDeviceCRC(void);

// HWI-FEATURE(resolution-presets) (M3-R4 window-modes): the typical sizes of the 16:9, 16:10 and 4:3 aspects (and the
//      exe's 5:4 1280x1024) listed on the Video screen beside the display's own mode; a sorted insert, the aspect's name
int rinAddModeIfMissing(rdevice* dev, int width, int height, int depth);   // 1 when added (ascending width, height)
const char* rinAspectName(int width, int height);                          // "4:3" | "16:10" | "16:9" | "5:4" | "W:H" reduced
int rinPresetCount(void);
void rinPreset(int index, int* width, int* height);

#ifdef __cplusplus
}
#endif

#endif

