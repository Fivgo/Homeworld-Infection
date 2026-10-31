/*=============================================================================
    Name    : VideoLayoutTest.c
    Purpose : HWI-TEST (video-layout): /catVideoLayoutTest <screen> (the Escape menu's Video screen: push it with
              /catFeScreen Video@<tick> in a game). Once <screen> has been the top screen for 20 frames, at any window
              size and UI scale, with no OS input:
              1. the layout: every control's and text's drawn extent (a text's string width and the font's height, a
                 check box's or radio button's 16x16 glyph, a slider's rail 11 beyond its rectangle on each side, a
                 button's art) against every box's frame (drawn 7 outside the box's rectangle): none may cross a frame
                 line; no two extents overlap (a button's own text excepted); the controls inside the panel; on the
                 Video screen TEXTURE INFO centred in its box and the UI SCALE / SMOOTH MOTION block above ACCEPT;
              2. the sliders' knobs: each slider set to 0, half and its maximum, the frame read back along the rail and
                 the knob's red centre found: its offset from the value's place (x0 + value / max * (x1 - x0)) must be
                 the same at the three values (within a logical pixel) and the knob inside the box's frame;
              3. the sliders' clicks, through the slider's own region processing (uicHorizSliderProcess with the
                 engine's mouse position, the press clipping it as the region does): a press at x0, the middle and x1
                 gives 0, half and the maximum; a drag past either end (the position clipped to the rectangle by
                 mouseClipPointToRect, as the clipped mouse would be) gives 0 and the value at x1 - 1 (the clip's last
                 column: one short of the maximum when the maximum is at least the width, as in the exe).
              The sliders' values (and the options they drive) and the mouse are put back. "videolayouttest:" lines,
              "videolayouttest: done ... result=PASS|FAIL"; then the engine quits.
    Created 2026-10-31 for Homeworld-Infection. Not Relic or Cataclysm code: the whole file is HWI-TEST.
=============================================================================*/

#include "VideoLayoutTest.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Debug.h"
#include "FEFlow.h"
#include "font.h"
#include "glinc.h"
#include "main.h"
#include "mouse.h"
#include "Region.h"
#include "StringSupport.h"
#include "UIControls.h"
#include "utility.h"

#define VL_SETTLE           20                  // frames the screen is shown before the checks
#define VL_SLIDERS          8
#define VL_EXTENTS          256
#define VL_FRAME            7                   // a box's frame line: 7 outside its rectangle
#define VL_OVERHANG         11                  // a slider's rail past each end of its rectangle

typedef struct
{
    real32 x0, y0, x1, y1;
    featom *atom;
}
vlextent;

static char vlScreen[64] = "";
static sdword vlStep = -2;                      // -2 off, -1 waiting, 0..2 the knob passes (0, half, max), 3 done
static sdword vlWait = 0;
static sdword vlChecks = 0, vlFails = 0;
static featom *vlSlider[VL_SLIDERS];
static featom *vlSliderBox[VL_SLIDERS];
static udword vlSliderValue[VL_SLIDERS];        // the values before the test
static real32 vlKnobOffset[3][VL_SLIDERS];      // the knob's centre minus the value's place, logical
static sdword vlSliders = 0;

void catVideoLayoutTestSet(char *screenName)
{
    snprintf(vlScreen, sizeof(vlScreen), "%s", screenName);
    vlStep = -1;
    vlWait = 0;
}

static void vlCheck(bool32 ok, const char *format, ...)
{
    char text[400];
    va_list args;

    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    vlChecks++;
    if (!ok)
    {
        vlFails++;
        dbgMessagef("videolayouttest: FAIL %s", text);
    }
}

static char *vlText(featom *atom)
{
    char *string = (char *)atom->pData;
    udword i;

    for (i = 0; i < strCurLanguage; i++)       // the language's string, as feStaticTextDraw takes it
    {
        string += strlen(string) + 1;
    }
    return string;
}

// the drawn extent of a control or a text (logical coordinates); FALSE for the rest
static bool32 vlExtent(featom *atom, vlextent *e)
{
    fonthandle save;
    sdword width, height;

    if (atom->region == NULL || bitTest(atom->flags, FAF_Hidden))
    {
        return FALSE;
    }
    e->atom = atom;
    switch (atom->type)
    {
        case FA_StaticText:
            save = fontMakeCurrent((fonthandle)atom->attribs);
            width = fontWidth(vlText(atom));
            height = fontHeight(vlText(atom));
            fontMakeCurrent(save);
            switch (atom->flags & FAM_Justification)
            {
                case FAM_JustRight:  e->x0 = (real32)(atom->x + atom->width - width); break;
                case FAM_JustCentre: e->x0 = (real32)(atom->x + (atom->width - width) / 2); break;
                default:             e->x0 = (real32)atom->x; break;
            }
            e->x1 = e->x0 + (real32)width;
            e->y0 = (real32)(atom->y + 2);         // the glyphs: the screens' fonts draw their capitals from y + 3 to
            e->y1 = (real32)(atom->y + height - 1);  // y + 10 (height 12; one row of shadow each side kept)
            return TRUE;
        case FA_CheckBox:
        case FA_RadioButton:
            e->x0 = (real32)atom->x;
            e->x1 = (real32)(atom->x + 16);
            e->y0 = (real32)atom->y;
            e->y1 = (real32)(atom->y + 16);
            return TRUE;
        case FA_HorizSlider:
            e->x0 = (real32)(atom->x - VL_OVERHANG);
            e->x1 = (real32)(atom->x + atom->width + VL_OVERHANG);
            e->y0 = (real32)(atom->y + 2);
            e->y1 = (real32)(atom->y + 15);
            return TRUE;
        case FA_Button:
            e->x0 = (real32)(atom->x + 1);
            e->x1 = (real32)(atom->x + atom->width - 1);
            e->y0 = (real32)(atom->y + 1);
            e->y1 = (real32)(atom->y + atom->height - 1);
            return TRUE;
        default:
            return FALSE;
    }
}

static bool32 vlBox(fescreen *screen, sdword index)
{
    featom *atom = &screen->atoms[index];

    return index > 0 && atom->type == 0 && atom->region != NULL && atom->pData == NULL &&
           bitTest(atom->flags, FAF_BorderVisible) && !bitTest(atom->flags, FAF_Background) &&
           !bitTest(atom->flags, FAF_Hidden);
}

static bool32 vlInside(vlextent *a, vlextent *b)
{
    return a->x0 >= b->x0 && a->x1 <= b->x1 && a->y0 >= b->y0 && a->y1 <= b->y1;
}

static bool32 vlOverlap(vlextent *a, vlextent *b)
{
    return a->x0 < b->x1 && b->x0 < a->x1 && a->y0 < b->y1 && b->y0 < a->y1;
}

static const char *vlName(featom *atom)
{
    if (atom->type == FA_StaticText && atom->pData != NULL)
    {
        return (char *)atom->pData;
    }
    return (atom->name != NULL) ? atom->name : "(unnamed)";
}

// 1. the layout
static void vlLayout(fescreen *screen)
{
    static vlextent e[VL_EXTENTS];
    sdword n = 0, i, j, boxes = 0;
    featom *panel = &screen->atoms[0], *accept = NULL;
    vlextent inner;

    for (i = 0; i < screen->nAtoms && n < VL_EXTENTS; i++)
    {
        if (vlExtent(&screen->atoms[i], &e[n]))
        {
            n++;
        }
        if (screen->atoms[i].name != NULL && strstr(screen->atoms[i].name, "_Accept") != NULL)
        {
            accept = &screen->atoms[i];
        }
    }
    // no extent crosses a box's frame line
    for (j = 0; j < screen->nAtoms; j++)
    {
        featom *b = &screen->atoms[j];
        real32 l, r, t, bt;

        if (!vlBox(screen, j))
        {
            continue;
        }
        boxes++;
        l = (real32)(b->x - VL_FRAME);
        r = (real32)(b->x + b->width + VL_FRAME);
        t = (real32)(b->y - VL_FRAME);
        bt = (real32)(b->y + b->height + VL_FRAME);
        for (i = 0; i < n; i++)
        {
            bool32 rows = e[i].y1 > t && e[i].y0 < bt, columns = e[i].x1 > l && e[i].x0 < r;
            bool32 cross = (rows && ((e[i].x0 < l && e[i].x1 > l) || (e[i].x0 < r && e[i].x1 > r))) ||
                           (columns && ((e[i].y0 < t && e[i].y1 > t) || (e[i].y0 < bt && e[i].y1 > bt)));

            vlCheck(!cross, "%s '%s' %.0f,%.0f-%.0f,%.0f crosses the frame of the box %d,%d-%d,%d (lines %.0f,%.0f-%.0f,%.0f)",
                    screen->name, vlName(e[i].atom), e[i].x0, e[i].y0, e[i].x1, e[i].y1, (int)b->x, (int)b->y,
                    (int)(b->x + b->width), (int)(b->y + b->height), l, t, r, bt);
        }
    }
    // no two extents overlap (a button's own text inside the button excepted)
    for (i = 0; i < n; i++)
    {
        for (j = i + 1; j < n; j++)
        {
            bool32 label = (e[i].atom->type == FA_Button && e[j].atom->type == FA_StaticText && vlInside(&e[j], &e[i])) ||
                           (e[j].atom->type == FA_Button && e[i].atom->type == FA_StaticText && vlInside(&e[i], &e[j]));

            vlCheck(label || !vlOverlap(&e[i], &e[j]), "%s '%s' %.0f,%.0f-%.0f,%.0f overlaps '%s' %.0f,%.0f-%.0f,%.0f",
                    screen->name, vlName(e[i].atom), e[i].x0, e[i].y0, e[i].x1, e[i].y1, vlName(e[j].atom), e[j].x0,
                    e[j].y0, e[j].x1, e[j].y1);
        }
    }
    // the controls inside the panel's frame (the base atom is the panel on the in-game screens)
    inner.x0 = (real32)(panel->x + 6);
    inner.y0 = (real32)(panel->y + 6);
    inner.x1 = (real32)(panel->x + panel->width - 6);
    inner.y1 = (real32)(panel->y + panel->height - 6);
    for (i = 0; bitTest(panel->flags, FAF_BorderVisible) && i < n; i++)
    {
        if (e[i].atom->type != FA_StaticText)
        {
            vlCheck(vlInside(&e[i], &inner), "%s '%s' %.0f,%.0f-%.0f,%.0f outside the panel", screen->name,
                    vlName(e[i].atom), e[i].x0, e[i].y0, e[i].x1, e[i].y1);
        }
    }
    if (strcmp(screen->name, "Video") == 0)
    {
        featom *button = feAtomFindInScreen(screen, "Video-TO-InGameVideo_Explanation"), *box = NULL;
        real32 lowest = 0.0f;

        for (j = 0; button != NULL && j < screen->nAtoms; j++)
        {
            featom *b = &screen->atoms[j];

            if (vlBox(screen, j) && b->x < button->x + button->width && button->x < b->x + b->width &&
                b->y < button->y + button->height && button->y < b->y + b->height)
            {
                box = b;
            }
        }
        if (button != NULL && box != NULL)
        {
            sdword left = button->x - box->x, right = box->x + box->width - button->x - button->width;
            sdword top = button->y - box->y, bottom = box->y + box->height - button->y - button->height;

            dbgMessagef("videolayouttest: TEXTURE INFO %d,%d-%d,%d in its box %d,%d-%d,%d: left %d right %d top %d bottom %d",
                        (int)button->x, (int)button->y, (int)(button->x + button->width), (int)(button->y + button->height),
                        (int)box->x, (int)box->y, (int)(box->x + box->width), (int)(box->y + box->height), (int)left,
                        (int)right, (int)top, (int)bottom);
            vlCheck(abs(left - right) <= 1 && abs(top - bottom) <= 1, "TEXTURE INFO centred in its box");
        }
        else
        {
            vlCheck(FALSE, "TEXTURE INFO and its box found");
        }
        for (i = 0; i < n; i++)
        {
            featom *a = e[i].atom;

            if ((a->name != NULL && (strcmp(a->name, "OP_UiScale") == 0 || strcmp(a->name, "OP_SmoothMotion") == 0)) ||
                (a->type == FA_StaticText && a->pData != NULL && (strcmp((char *)a->pData, "UI SCALE") == 0 ||
                                                                strcmp((char *)a->pData, "SMOOTH MOTION") == 0)))
            {
                lowest = max(lowest, e[i].y1);
            }
        }
        dbgMessagef("videolayouttest: UI SCALE / SMOOTH MOTION block ends at %.0f, ACCEPT at %d", lowest,
                    (accept != NULL) ? (int)accept->y : -1);
        vlCheck(accept != NULL && lowest > 0.0f && lowest + 4.0f <= (real32)accept->y,
                "the UI SCALE / SMOOTH MOTION block clear of ACCEPT");
    }
    // the sliders and their boxes
    vlSliders = 0;
    for (i = 0; i < screen->nAtoms && vlSliders < VL_SLIDERS; i++)
    {
        featom *s = &screen->atoms[i];

        if (s->type != FA_HorizSlider || s->region == NULL)
        {
            continue;
        }
        vlSliderBox[vlSliders] = NULL;
        for (j = 0; j < screen->nAtoms; j++)
        {
            featom *b = &screen->atoms[j];

            if (vlBox(screen, j) && b->y <= s->y + 8 && s->y + 8 <= b->y + b->height &&
                b->x < s->x + s->width && s->x < b->x + b->width)
            {
                vlSliderBox[vlSliders] = b;
            }
        }
        vlSlider[vlSliders] = s;
        vlSliderValue[vlSliders] = ((sliderhandle)s->region)->value;
        dbgMessagef("videolayouttest: slider %s %d,%d-%d (rail %d-%d) max %d value %d box %d-%d", s->name, (int)s->x,
                    (int)s->y, (int)(s->x + s->width), (int)(s->x - VL_OVERHANG), (int)(s->x + s->width + VL_OVERHANG),
                    (int)((sliderhandle)s->region)->maxvalue, (int)vlSliderValue[vlSliders],
                    vlSliderBox[vlSliders] ? (int)vlSliderBox[vlSliders]->x : -1,
                    vlSliderBox[vlSliders] ? (int)(vlSliderBox[vlSliders]->x + vlSliderBox[vlSliders]->width) : -1);
        vlSliders++;
    }
    dbgMessagef("videolayouttest: %s layout extents=%d boxes=%d sliders=%d logical=%dx%d physical=%dx%d", screen->name,
                (int)n, (int)boxes, (int)vlSliders, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight,
                (int)MAIN_WindowWidthActual, (int)MAIN_WindowHeightActual);
}

static udword vlStepValue(sdword step, udword max)
{
    return (step == 0) ? 0 : ((step == 1) ? max / 2 : max);
}

// 2. the knob along the rail of the frame just drawn: its red centre, logical x (< 0: not found)
static real32 vlKnobRead(featom *s)
{
    static ubyte row[3 * 8192];
    real32 sx = (real32)MAIN_WindowWidthActual / (real32)MAIN_WindowWidth;
    real32 sy = (real32)MAIN_WindowHeightActual / (real32)MAIN_WindowHeight;
    sdword py = MAIN_WindowHeightActual - 1 - (sdword)((s->y + 9) * sy);
    sdword a = (sdword)((s->x - 2 * VL_OVERHANG) * sx), b = (sdword)((s->x + s->width + 2 * VL_OVERHANG) * sx), x;
    real32 sum = 0.0f, count = 0.0f;

    if (MAIN_WindowWidthActual > 8192 || py < 0 || py >= MAIN_WindowHeightActual)
    {
        return -1.0f;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, py, MAIN_WindowWidthActual, 1, GL_RGB, GL_UNSIGNED_BYTE, row);
    for (x = max(0, a); x < min(MAIN_WindowWidthActual, b); x++)
    {
        ubyte *p = &row[3 * x];

        if (p[0] > 180 && p[1] < 130 && p[2] < 130)
        {
            sum += (real32)x;
            count += 1.0f;
        }
    }
    return (count > 0.0f) ? (sum / count + 0.5f) / sx : -1.0f;
}

static void vlSet(sdword step)
{
    sdword i;

    for (i = 0; i < vlSliders; i++)
    {
        sliderhandle h = (sliderhandle)vlSlider[i]->region;

        h->value = vlStepValue(step, h->maxvalue);
    }
}

// 3. the clicks and drags through the region's own processing
static void vlClicks(void)
{
    sdword i, saveX = mouseCursorXPosition, saveY = mouseCursorYPosition;

    for (i = 0; i < vlSliders; i++)
    {
        featom *s = vlSlider[i];
        regionhandle region = (regionhandle)s->region;
        sliderhandle h = (sliderhandle)region;
        sdword x0 = region->rect.x0, x1 = region->rect.x1, y = (region->rect.y0 + region->rect.y1) / 2, px, py;
        udword at[3], drag[2], right;
        sdword where[3] = { x0, (x0 + x1) / 2, x1 }, past[2] = { x0 - 40, x1 + 40 }, k;

        for (k = 0; k < 3; k++)
        {
            mouseCursorXPosition = where[k];
            mouseCursorYPosition = y;
            region->processFunction(region, region->userID, RPE_PressLeft, 0);
            at[k] = h->value;
            region->processFunction(region, region->userID, RPE_ReleaseLeft, 0);
        }
        for (k = 0; k < 2; k++)
        {
            mouseCursorXPosition = (x0 + x1) / 2;
            mouseCursorYPosition = y;
            region->processFunction(region, region->userID, RPE_PressLeft, 0);
            px = past[k];
            py = y;
            mouseClipPointToRect(&px, &py, &region->rect);   // the clipped mouse of a drag past the end
            mouseCursorXPosition = px;
            mouseCursorYPosition = py;
            region->processFunction(region, region->userID, RPE_HoldLeft, 0);
            drag[k] = h->value;
            region->processFunction(region, region->userID, RPE_ReleaseLeft, 0);
        }
        // a drag stops at the rectangle's last column (mouseClipPointToRect: x1 - 1), so its right end is the value
        // there, one short of the maximum when max >= the width (the exe's clip and mapping, unchanged); the press at
        // x1 itself (the region takes x <= x1) gives the maximum
        right = (udword)((real32)h->maxvalue * (real32)(x1 - 1 - x0) / (real32)(x1 - x0));
        dbgMessagef("videolayouttest: clicks %s at x0 %d -> %d, middle %d -> %d, x1 %d -> %d (max %d); drags past the ends -> %d, %d (the clip's x1 - 1: %d)",
                    s->name, (int)where[0], (int)at[0], (int)where[1], (int)at[1], (int)where[2], (int)at[2],
                    (int)h->maxvalue, (int)drag[0], (int)drag[1], (int)right);
        vlCheck(at[0] == 0 && at[2] == h->maxvalue && abs((sdword)at[1] - (sdword)(h->maxvalue / 2)) <= 1,
                "%s: a click at x0 / the middle / x1 gives 0 / half / max", s->name);
        vlCheck(drag[0] == 0 && drag[1] == right && right + 2 >= h->maxvalue,
                "%s: a drag past the ends gives 0 and the value at x1 - 1", s->name);
        // the value and the option it drives back
        h->value = vlSliderValue[i];
        if (h->processFunction != NULL)
        {
            h->processFunction(region, region->userID, RPE_PressLeft, 0);
        }
    }
    mouseCursorXPosition = saveX;
    mouseCursorYPosition = saveY;
    mouseClipToRect(NULL);
}

void catVideoLayoutFrameEnd(void)
{
    fescreen *top = (feStackIndex >= 0) ? feStack[feStackIndex].screen : NULL;
    sdword i;

    if (vlStep < -1 || vlStep > 2)
    {
        return;
    }
    if (top == NULL || strcmp(top->name, vlScreen) != 0)
    {
        vlWait = 0;
        return;
    }
    if (vlStep == -1)
    {
        if (++vlWait < VL_SETTLE)
        {
            return;
        }
        vlLayout(top);
        vlStep = 0;
        vlSet(0);                               // the next frame draws the knobs at 0
        return;
    }
    // the frame of knob pass vlStep was just drawn
    for (i = 0; i < vlSliders; i++)
    {
        featom *s = vlSlider[i];
        udword max = ((sliderhandle)s->region)->maxvalue, value = vlStepValue(vlStep, max);
        real32 place = (real32)s->x + (real32)value / (real32)max * (real32)s->width;
        real32 knob = vlKnobRead(s);

        vlKnobOffset[vlStep][i] = knob - place;
        dbgMessagef("videolayouttest: knob %s value %d/%d place %.2f knob %.2f offset %.2f", s->name, (int)value, (int)max,
                    (double)place, (double)knob, (double)(knob - place));
        vlCheck(knob >= 0.0f, "%s: the knob found at value %d", s->name, (int)value);
        if (vlStep > 0)
        {
            vlCheck(fabsf(vlKnobOffset[vlStep][i] - vlKnobOffset[0][i]) <= 1.0f,
                    "%s: the knob at value %d where the value is (offset %.2f, at 0 %.2f)", s->name, (int)value,
                    (double)vlKnobOffset[vlStep][i], (double)vlKnobOffset[0][i]);
        }
        if (vlSliderBox[i] != NULL)
        {
            vlCheck(knob - 6.0f >= (real32)(vlSliderBox[i]->x - VL_FRAME + 1) &&
                    knob + 6.0f <= (real32)(vlSliderBox[i]->x + vlSliderBox[i]->width + VL_FRAME - 1),
                    "%s: the knob at value %d inside its box's frame", s->name, (int)value);
        }
    }
    vlStep++;
    if (vlStep <= 2)
    {
        vlSet(vlStep);
        return;
    }
    vlClicks();
    dbgMessagef("videolayouttest: done screen=%s logical=%dx%d physical=%dx%d sliders=%d checks=%d fails=%d result=%s",
                vlScreen, (int)MAIN_WindowWidth, (int)MAIN_WindowHeight, (int)MAIN_WindowWidthActual,
                (int)MAIN_WindowHeightActual, (int)vlSliders, (int)vlChecks, (int)vlFails, (vlFails == 0) ? "PASS" : "FAIL");
    utyGameQuit(NULL, NULL);
}
