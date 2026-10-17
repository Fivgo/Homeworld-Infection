/*=============================================================================
    Name    : KeyBindings.c
    Purpose : This file handles all of the logic for the key bindings

    Created 8/04/1999 by Drew Dunlop
    Copyright Relic Entertainment, Inc.  All rights reserved.
=============================================================================*/

#include "KeyBindings.h"

#include "Debug.h"
#include "FEColour.h"
#include "FontReg.h"
#include "Globals.h"
#include "StringsOnly.h"
#include "StringSupport.h"
#include "Types.h"
#include "UIControls.h"

/*=============================================================================
    Defines :
=============================================================================*/


/*=============================================================================
    Data definitions :
=============================================================================*/

udword kbKeySavedKeys[20] =
{
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

kbBoundKeys *kbKeyTable = NULL;

kbBoundKeys kbKeyTableEnglish[kbTOTAL_COMMANDS] =
{  // command                 defaultkey      primary key     key to reset to
    { kbNEXT_FORMATION      , TABKEY        , TABKEY        , TABKEY       , KB_NormalBound },
    { kbBUILD_MANAGER       , BKEY          , BKEY          , BKEY         , KB_NormalBound },
    { kbPREVIOUS_FOCUS      , CKEY          , CKEY          , CKEY         , KB_NormalBound },
    { kbNEXT_FOCUS          , VKEY          , VKEY          , VKEY         , KB_NormalBound },
    { kbDOCK                , DKEY          , DKEY          , DKEY         , KB_NormalBound },
    { kbSELECT_ALL_VISIBLE  , EKEY          , EKEY          , EKEY         , KB_NormalBound },
    { kbFOCUS               , FKEY          , FKEY          , FKEY         , KB_NormalBound },
    { kbRESEARCH_MANAGER    , RKEY          , RKEY          , RKEY         , KB_NormalBound },
    { kbHARVEST             , HKEY2         , HKEY2         , HKEY2        , KB_NormalBound },
    { kbMOVE                , MKEY          , MKEY          , MKEY         , KB_NormalBound },
    { kbNEXT_TACTIC         , RBRACK        , RBRACK        , RBRACK       , KB_NormalBound },
    { kbPREVIOUS_TACTIC     , LBRACK        , LBRACK        , LBRACK       , KB_NormalBound },
    { kbSCUTTLE             , SKEY          , SKEY          , SKEY         , KB_NormalBound },
    { kbSHIP_SPECIAL        , ZKEY          , ZKEY          , ZKEY         , KB_NormalBound },
    { kbTACTICAL_OVERLAY    , CAPSLOCKKEY   , CAPSLOCKKEY   , CAPSLOCKKEY  , KB_NormalBound },
//    { kbSENSORS_MANAGER     , SPACEKEY      , SPACEKEY      , SPACEKEY     , KB_NormalBound },
    { kbMOTHERSHIP          , HOMEKEY       , HOMEKEY       , HOMEKEY      , KB_NormalBound },
    { kbKAMIKAZE            , KKEY          , KKEY          , KKEY         , KB_NormalBound },
    { kbCANCEL_ORDERS       , TILDEKEY      , TILDEKEY      , TILDEKEY     , KB_NormalBound },
    { kbLAUNCH_MANAGER      , LKEY          , LKEY          , LKEY         , KB_NormalBound },
};

kbBoundKeys kbKeyTableFrench[kbTOTAL_COMMANDS] =
{  // command                 defaultkey      primary key     key to reset to
    { kbNEXT_FORMATION      , TABKEY        , TABKEY        , TABKEY       , KB_NormalBound },
    { kbBUILD_MANAGER       , BKEY          , BKEY          , BKEY         , KB_NormalBound },
    { kbPREVIOUS_FOCUS      , CKEY          , XKEY          , XKEY         , KB_NormalBound },
    { kbNEXT_FOCUS          , VKEY          , VKEY          , VKEY         , KB_NormalBound },
    { kbDOCK                , DKEY          , AKEY          , AKEY         , KB_NormalBound },
    { kbSELECT_ALL_VISIBLE  , EKEY          , EKEY          , EKEY         , KB_NormalBound },
    { kbFOCUS               , FKEY          , FKEY          , FKEY         , KB_NormalBound },
    { kbRESEARCH_MANAGER    , RKEY          , RKEY          , RKEY         , KB_NormalBound },
    { kbHARVEST             , HKEY2         , CKEY          , CKEY         , KB_NormalBound },
    { kbMOVE                , MKEY          , DKEY          , DKEY         , KB_NormalBound },
    { kbNEXT_TACTIC         , RBRACK        , RBRACK        , RBRACK       , KB_NormalBound },
    { kbPREVIOUS_TACTIC     , LBRACK        , LBRACK        , LBRACK       , KB_NormalBound },
    { kbSCUTTLE             , SKEY          , SKEY          , SKEY         , KB_NormalBound },
    { kbSHIP_SPECIAL        , ZKEY          , ZKEY          , ZKEY         , KB_NormalBound },
    { kbTACTICAL_OVERLAY    , CAPSLOCKKEY   , CAPSLOCKKEY   , CAPSLOCKKEY  , KB_NormalBound },
//    { kbSENSORS_MANAGER     , SPACEKEY      , SPACEKEY      , SPACEKEY     , KB_NormalBound },
    { kbMOTHERSHIP          , HOMEKEY       , HOMEKEY       , HOMEKEY      , KB_NormalBound },
    { kbKAMIKAZE            , KKEY          , KKEY          , KKEY         , KB_NormalBound },
    { kbCANCEL_ORDERS       , TILDEKEY      , TILDEKEY      , TILDEKEY     , KB_NormalBound },
    { kbLAUNCH_MANAGER      , LKEY          , LKEY          , LKEY         , KB_NormalBound },
};

kbBoundKeys kbKeyTableGerman[kbTOTAL_COMMANDS] =
{  // command                 defaultkey      primary key     key to reset to
    { kbNEXT_FORMATION      , TABKEY        , TABKEY        , TABKEY       , KB_NormalBound },
    { kbBUILD_MANAGER       , BKEY          , BKEY          , BKEY         , KB_NormalBound },
    { kbPREVIOUS_FOCUS      , CKEY          , CKEY          , CKEY         , KB_NormalBound },
    { kbNEXT_FOCUS          , VKEY          , VKEY          , VKEY         , KB_NormalBound },
    { kbDOCK                , DKEY          , DKEY          , DKEY         , KB_NormalBound },
    { kbSELECT_ALL_VISIBLE  , EKEY          , EKEY          , EKEY         , KB_NormalBound },
    { kbFOCUS               , FKEY          , FKEY          , FKEY         , KB_NormalBound },
    { kbRESEARCH_MANAGER    , RKEY          , RKEY          , RKEY         , KB_NormalBound },
    { kbHARVEST             , HKEY2         , HKEY2         , HKEY2        , KB_NormalBound },
    { kbMOVE                , MKEY          , WKEY          , WKEY         , KB_NormalBound },
    { kbNEXT_TACTIC         , RBRACK        , RBRACK        , RBRACK       , KB_NormalBound },
    { kbPREVIOUS_TACTIC     , LBRACK        , LBRACK        , LBRACK       , KB_NormalBound },
    { kbSCUTTLE             , SKEY          , SKEY          , SKEY         , KB_NormalBound },
    { kbSHIP_SPECIAL        , ZKEY          , ZKEY          , ZKEY         , KB_NormalBound },
    { kbTACTICAL_OVERLAY    , CAPSLOCKKEY   , CAPSLOCKKEY   , CAPSLOCKKEY  , KB_NormalBound },
//    { kbSENSORS_MANAGER     , SPACEKEY      , SPACEKEY      , SPACEKEY     , KB_NormalBound },
    { kbMOTHERSHIP          , HOMEKEY       , HOMEKEY       , HOMEKEY      , KB_NormalBound },
    { kbKAMIKAZE            , KKEY          , KKEY          , KKEY         , KB_NormalBound },
    { kbCANCEL_ORDERS       , TILDEKEY      , TILDEKEY      , TILDEKEY     , KB_NormalBound },
    { kbLAUNCH_MANAGER      , LKEY          , LKEY          , LKEY         , KB_NormalBound },
};

kbBoundKeys kbKeyTableSpanish[kbTOTAL_COMMANDS] =
{  // command                 defaultkey      primary key     key to reset to
    { kbNEXT_FORMATION      , TABKEY        , TABKEY        , TABKEY       , KB_NormalBound },
    { kbBUILD_MANAGER       , BKEY          , BKEY          , BKEY         , KB_NormalBound },
    { kbPREVIOUS_FOCUS      , CKEY          , CKEY          , CKEY         , KB_NormalBound },
    { kbNEXT_FOCUS          , VKEY          , VKEY          , VKEY         , KB_NormalBound },
    { kbDOCK                , DKEY          , AKEY          , AKEY         , KB_NormalBound },
    { kbSELECT_ALL_VISIBLE  , EKEY          , EKEY          , EKEY         , KB_NormalBound },
    { kbFOCUS               , FKEY          , FKEY          , FKEY         , KB_NormalBound },
    { kbRESEARCH_MANAGER    , RKEY          , RKEY          , RKEY         , KB_NormalBound },
    { kbHARVEST             , HKEY2         , HKEY2         , HKEY2        , KB_NormalBound },
    { kbMOVE                , MKEY          , DKEY          , DKEY         , KB_NormalBound },
    { kbNEXT_TACTIC         , RBRACK        , RBRACK        , RBRACK       , KB_NormalBound },
    { kbPREVIOUS_TACTIC     , LBRACK        , LBRACK        , LBRACK       , KB_NormalBound },
    { kbSCUTTLE             , SKEY          , SKEY          , SKEY         , KB_NormalBound },
    { kbSHIP_SPECIAL        , ZKEY          , ZKEY          , ZKEY         , KB_NormalBound },
    { kbTACTICAL_OVERLAY    , CAPSLOCKKEY   , CAPSLOCKKEY   , CAPSLOCKKEY  , KB_NormalBound },
//    { kbSENSORS_MANAGER     , SPACEKEY      , SPACEKEY      , SPACEKEY     , KB_NormalBound },
    { kbMOTHERSHIP          , HOMEKEY       , HOMEKEY       , HOMEKEY      , KB_NormalBound },
    { kbKAMIKAZE            , KKEY          , KKEY          , KKEY         , KB_NormalBound },
    { kbCANCEL_ORDERS       , TILDEKEY      , TILDEKEY      , TILDEKEY     , KB_NormalBound },
    { kbLAUNCH_MANAGER      , LKEY          , LKEY          , LKEY         , KB_NormalBound },
};

kbBoundKeys kbKeyTableItalian[kbTOTAL_COMMANDS] =
{  // command                 defaultkey      primary key     key to reset to
    { kbNEXT_FORMATION      , TABKEY        , TABKEY        , TABKEY       , KB_NormalBound },
    { kbBUILD_MANAGER       , BKEY          , BKEY          , BKEY         , KB_NormalBound },
    { kbPREVIOUS_FOCUS      , CKEY          , CKEY          , CKEY         , KB_NormalBound },
    { kbNEXT_FOCUS          , VKEY          , VKEY          , VKEY         , KB_NormalBound },
    { kbDOCK                , DKEY          , AKEY          , AKEY         , KB_NormalBound },
    { kbSELECT_ALL_VISIBLE  , EKEY          , EKEY          , EKEY         , KB_NormalBound },
    { kbFOCUS               , FKEY          , FKEY          , FKEY         , KB_NormalBound },
    { kbRESEARCH_MANAGER    , RKEY          , RKEY          , RKEY         , KB_NormalBound },
    { kbHARVEST             , HKEY2         , HKEY2         , HKEY2        , KB_NormalBound },
    { kbMOVE                , MKEY          , MKEY          , MKEY         , KB_NormalBound },
    { kbNEXT_TACTIC         , RBRACK        , RBRACK        , RBRACK       , KB_NormalBound },
    { kbPREVIOUS_TACTIC     , LBRACK        , LBRACK        , LBRACK       , KB_NormalBound },
    { kbSCUTTLE             , SKEY          , SKEY          , SKEY         , KB_NormalBound },
    { kbSHIP_SPECIAL        , ZKEY          , ZKEY          , ZKEY         , KB_NormalBound },
    { kbTACTICAL_OVERLAY    , CAPSLOCKKEY   , CAPSLOCKKEY   , CAPSLOCKKEY  , KB_NormalBound },
//    { kbSENSORS_MANAGER     , SPACEKEY      , SPACEKEY      , SPACEKEY     , KB_NormalBound },
    { kbMOTHERSHIP          , HOMEKEY       , HOMEKEY       , HOMEKEY      , KB_NormalBound },
    { kbKAMIKAZE            , KKEY          , KKEY          , KKEY         , KB_NormalBound },
    { kbCANCEL_ORDERS       , TILDEKEY      , TILDEKEY      , TILDEKEY     , KB_NormalBound },
    { kbLAUNCH_MANAGER      , LKEY          , LKEY          , LKEY         , KB_NormalBound },
};


kbBoundKeys kbKeySaveTable[kbTOTAL_COMMANDS];

// This is a look-up table that has a value of TRUE for
bool8 kbCanMapKey[KEY_TOTAL_KEYS];

// This is a look-up table for the strings
sdword kbKeyToString[KEY_TOTAL_KEYS];

// fonthandle for the keyboard list window font initialized in the options init
fonthandle kbKeyListFont;

// pointer to the listwindow handle
listwindowhandle kbListWindow=NULL;

bool32 bkDisableKeyRemap=FALSE;

/*=============================================================================
    Private Function Prototypes :
=============================================================================*/

void kbSetNewKey(udword keypressed);
bool32 kbKeyUsed(udword keytocheck);

/*=============================================================================
    Function Logic :
=============================================================================*/


/*-----------------------------------------------------------------------------
    Name        : kbListTitleDraw
    Description : This function draws the title for the keyboard list window
    Inputs      : Rectangle of draw area
    Outputs     : none
    Parameters  : rectangle *rect
    Return      : void
-----------------------------------------------------------------------------*/
void kbListTitleDraw(rectangle *rect)
{
    fonthandle oldfont;
    rectangle r = *rect;

    oldfont = fontMakeCurrent(kbKeyListFont);

    fontPrintf(rect->x0+6, rect->y0, colWhite, strGetString(strCommandToBind));
    fontPrintf(rect->x0 + (((rect->x1-rect->x0)*3)/5), rect->y0, colWhite, strGetString(strKeyBound));

    r.x0 -= 2;
    r.x1++;
    r.y1--;

    primRectTranslucent2(&r, colRGBA(0,160,100,63));
//    primLine2(r.x0,r.y1,r.x1,r.y1,colRGB(20,200,20));
//    primRectOutline2(&r, 1, colRGB(20,200,20));

    fontMakeCurrent(oldfont);
}


/*-----------------------------------------------------------------------------
    Name        : kbListItemDraw
    Description : This function draws each item in the list window
    Inputs      : rect, and the item pointer
    Outputs     : none
    Parameters  : rectangle *rect, listitemhandle data
    Return      : void
-----------------------------------------------------------------------------*/
void kbListItemDraw(rectangle *rect, listitemhandle data)
{
    fonthandle      oldfont;
    kbBoundKeys    *bkey = (kbBoundKeys *)data->data;
    color           c = FEC_ListItemStandard;

    oldfont = fontMakeCurrent(kbKeyListFont);

    // if selected then this color
    if (bitTest(data->flags, UICLI_Selected))
    {
        c = FEC_ListItemSelected;
    }

    if (bkey->status == KB_NotBound)
    {
        // if not bound then print in red
        c = colRGB(255,20,20);
    }
    else if (bkey->status == KB_GetKeyPress)
    {
        // if not bound then print in yellow color
        c = colRGB(255,255,20);
    }

    fontPrintf(rect->x0 + 6, rect->y0, c, strGetString(bkey->command + strKeyCommandOffset));

    if (bkey->status == KB_GetKeyPress)
    {
        // if getting keypress then show it with ????
        fontPrintf(rect->x0 + (((rect->x1-rect->x0)*3)/5), rect->y0, c, "??????");
    }
    else
    {
        if (bkey->primarykey == 0)
        {
            // no key assigned to this
            fontPrintf(rect->x0 + (((rect->x1-rect->x0)*3)/5), rect->y0, c, "%s", strGetString(strNoKeyBound));
        }
        else
        {
            // key assigned so print the name of that key
            dbgAssertOrIgnore(bkey->primarykey < KEY_TOTAL_KEYS);
            fontPrintf(rect->x0 + (((rect->x1-rect->x0)*3)/5), rect->y0, c, "%s", strGetString(kbKeyToString[bkey->primarykey]));
        }
    }

    fontMakeCurrent(oldfont);
}


/*-----------------------------------------------------------------------------
    Name        : kbListWindowCB
    Description : This function initializes the key bindings list window
    Inputs      : name, and atom
    Outputs     : none
    Parameters  : char *string, featom *atom
    Return      : void
-----------------------------------------------------------------------------*/
void kbListWindowCB(char *string, featom *atom)
{
    fonthandle oldfont;
    sdword     index;
    kbBoundKeys    *bkey;

    if (FEFIRSTCALL(atom))
    {
        oldfont = fontMakeCurrent(kbKeyListFont);
        kbListWindow = (listwindowhandle)atom->pData;

        uicListWindowInit(kbListWindow,
                          kbListTitleDraw,                      //  title draw
                          NULL,                                 //  title click process, no title
                          fontHeight(" ")+(fontHeight(" ")>>1), //  title height, no title
                          kbListItemDraw,                       // item draw funtcion
                          fontHeight(" ")+(fontHeight(" ")>>1),       // item height
                          UICLW_CanSelect|UICLW_CanHaveFocus);

        for (index = 0; index < kbTOTAL_COMMANDS; index++)
        {
            uicListAddItem(kbListWindow, (ubyte *)&kbKeyTable[index], UICLI_CanSelect, UICLW_AddToHead);
        }

        fontMakeCurrent(oldfont);
        return;
    }
    else if (FELASTCALL(atom))
    {
        bitClear(kbListWindow->windowflags, UICLW_GetKeyPressed);

        for (index = 0; index < kbTOTAL_COMMANDS; index++)
        {
            if (kbKeyTable[index].primarykey == 0)
            {
                kbKeyTable[index].status = KB_NotBound;
            }
            else
            {
                kbKeyTable[index].status = KB_NormalBound;
            }
        }

        kbListWindow = NULL;
        return;
    }
    // General callbacks because of user input
    switch (kbListWindow->message)
    {
        case CM_AcceptText:
        case CM_DoubleClick:
            bkey = (kbBoundKeys *)kbListWindow->CurLineSelected->data;
            bitSet(kbListWindow->windowflags, UICLW_GetKeyPressed);
#ifdef DEBUG_STOMP
            regVerify((regionhandle)&kbListWindow->reg);
#endif
            bitSet(kbListWindow->reg.status, RSF_DrawThisFrame);
            bkey->status = KB_GetKeyPress;
            break;
        case CM_KeyCaptured:
            kbSetNewKey(kbListWindow->keypressed);
            break;
        case CM_LoseFocus:
        case CM_NewItemSelected:
            bitClear(kbListWindow->windowflags, UICLW_GetKeyPressed);
            for (index = 0; index < kbTOTAL_COMMANDS; index++)
            {
                if (kbKeyTable[index].primarykey == 0)
                {
                    kbKeyTable[index].status = KB_NotBound;
                }
                else
                {
                    kbKeyTable[index].status = KB_NormalBound;
                }
            }
            break;
    }
}


/*-----------------------------------------------------------------------------
    Name        : kbPoolListItemDraw
    Description : This function draws each item in the key pool list window
    Inputs      : rect, and the item pointer
    Outputs     : none
    Parameters  : rectangle *rect, listitemhandle data
    Return      : void
-----------------------------------------------------------------------------*/
void kbPoolListItemDraw(rectangle *rect, listitemhandle data)
{
    fonthandle      oldfont;
    color           c = FEC_ListItemStandard;
    sword           key1, key2;
    udword          udata = (udword)data->data;

    oldfont = fontMakeCurrent(kbKeyListFont);

    // unpack the two key's
    key1 = (sword) (udata&(0x0000FFFF));
    key2 = (sword) ((udata&(0xFFFF0000))>>16);

    // if selected then this color
    if (bitTest(data->flags, UICLI_Selected))
    {
        c = FEC_ListItemSelected;
    }

    if (key1 != -1)
    {
        fontPrintf(rect->x0 + 6, rect->y0, c, strGetString(kbKeyToString[key1]));
    }

    if (key2 != -1)
    {
        fontPrintf(rect->x0 + ( (rect->x1-rect->x0)>>1 ), rect->y0, c, strGetString(kbKeyToString[key2]));
    }

    fontMakeCurrent(oldfont);
}

/*-----------------------------------------------------------------------------
    Name        : kbPoolListWindowCB
    Description : This function Initializes the list of keys window
    Inputs      : keypressed
    Outputs     : none
    Parameters  : udword keypressed
    Return      : void
-----------------------------------------------------------------------------*/
void kbPoolListWindowCB(char *string, featom *atom)
{
    fonthandle          oldfont;
    static sdword       index;
    listwindowhandle    keypool;
    sdword              done;
    udword              data;
    sword               key1 = 0, key2 = 0;

    if (FEFIRSTCALL(atom))
    {
        oldfont = fontMakeCurrent(kbKeyListFont);
        keypool = (listwindowhandle)atom->pData;

        uicListWindowInit(keypool,
                          NULL,                      //  title draw
                          NULL,                      //  title click process, no title
                          0,                         //  title height, no title
                          kbPoolListItemDraw,                       // item draw funtcion
                          fontHeight(" ")+(fontHeight(" ")>>1),       // item height
                          UICLW_CanSelect|UICLW_CanHaveFocus);

        done = 0;
        for (index=0;index<KEY_TOTAL_KEYS;index++)
        {
            if (kbCanMapKey[index] == TRUE)
            {
                if (done == 0)
                {
                    key1 = (sword)index;
                    done = 1;
                }
                else if (done == 1)
                {
                    key2 = (sword)index;
                    done = 2;
                }
            }

            if (done == 2)
            {
                // pack the two 16-bit swords into the on udword
                data = (udword) ( (key1)| ((key2<<16)) );
                key1 = -1;
                key2 = -1;
                done = 0;
                uicListAddItem(keypool, (ubyte *)data,  UICLI_CanSelect, UICLW_AddToTail);
            }
        }
        if (done == 1)
        {
            // pack the two 16-bit swords into the on udword
            key2 = -1;
            data = (udword) ( (key1)| ((key2<<16)) );
            key1 = -1;
            done = 0;
            uicListAddItem(keypool, (ubyte *)data,  UICLI_CanSelect, UICLW_AddToTail);
        }

        fontMakeCurrent(oldfont);
        return;
    }
    else if (FELASTCALL(atom))
    {
        return;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kbSetNewKey
    Description : This function sets the new keypressed to the currently selected command.
    Inputs      : keypressed
    Outputs     : none
    Parameters  : udword keypressed
    Return      : void
-----------------------------------------------------------------------------*/
void kbSetNewKey(udword keypressed)
{
    kbBoundKeys *bkey;
    sdword       index;

    keypressed&=0x000000FF;

    dbgAssertOrIgnore(keypressed < KEY_TOTAL_KEYS);

    bkey = (kbBoundKeys *)kbListWindow->CurLineSelected->data;
    if (kbCanMapKey[keypressed])
    {
        for (index=0;index<kbTOTAL_COMMANDS;index++)
        {
            if (kbKeyTable[index].primarykey == keypressed)
            {
                kbKeyTable[index].status = KB_NotBound;
                kbKeyTable[index].primarykey = 0;
            }
        }
        bkey->primarykey = keypressed;
        bkey->status = KB_NormalBound;
#ifdef DEBUG_STOMP
        regVerify((regionhandle)&kbListWindow->reg);
#endif
        bitSet(kbListWindow->reg.status, RSF_DrawThisFrame);
    }
    else
    {
        if (bkey->primarykey != 0)
            bkey->status = KB_NormalBound;
        else
            bkey->status = KB_NotBound;

        if (gameIsRunning)
        {
            feScreenStart(feStack[feStackIndex].baseRegion, "In_Game_Invalid_Key");
        }
        else
        {
            feScreenStart(feStack[feStackIndex].baseRegion, "Invalid_Key");
        }
        // this key is not a key that can be remapped.
    }
}


/*-----------------------------------------------------------------------------
    Name        : kbKeyResetToDefault
    Description : This function will reset all of the keyboard bindings to
                  default.
    Inputs      : name, and atom
    Outputs     : none
    Parameters  : char *string, featom *atom
    Return      : void
-----------------------------------------------------------------------------*/
void kbKeyResetToDefault(char *string, featom *atom)
{
    sdword index;

    for (index=kbTOTAL_COMMANDS-1; index >= 0; index--)
    {
        kbKeyTable[index].primarykey = kbKeyTable[index].resettokey;
        kbKeyTable[index].status = KB_NormalBound;
    }

#ifdef DEBUG_STOMP
    regVerify(&kbListWindow->reg);
#endif
    bitSet(kbListWindow->reg.status, RSF_DrawThisFrame);
}


/*-----------------------------------------------------------------------------
    Name        : kbCheckBindings
    Description : This function will checks to see if this key is a remappable
                  key and then do the conversion.
    Inputs      : keypressed
    Outputs     : none
    Parameters  : sdword keypressed
    Return      : void
-----------------------------------------------------------------------------*/
sdword kbCheckBindings(sdword keypressed)
{
    sdword index;

    if (kbCanMapKey[keypressed])
    {
        for (index=0;index<kbTOTAL_COMMANDS;index++)
        {
            if (kbKeyTable[index].primarykey == keypressed)
            {
                return (kbKeyTable[index].defaultkey);
            }
        }

        // key not bound to anything !!!!!
        return(0);
    }
    else
    {
        return (keypressed);
    }
}


/*-----------------------------------------------------------------------------
    Name        : kbRestoreSavedSettings
    Description : This function will restore the previosly saved settings.
    Inputs      : none
    Outputs     : none
    Parameters  : void
    Return      : void
-----------------------------------------------------------------------------*/
void kbRestoreSavedSettings(void)
{
    sdword index;

    for (index=0; index < kbTOTAL_COMMANDS; index++)
    {
        kbKeyTable[index] = kbKeySaveTable[index];
    }
}


/*-----------------------------------------------------------------------------
    Name        : kbSaveSettings
    Description : This function will save the current settings to a temporary
                  structure.  This is called when the options are started so
                  That a user can cancel any changes they don't like.
    Inputs      : void
    Outputs     : none
    Parameters  : void
    Return      : void
-----------------------------------------------------------------------------*/
void kbSaveSettings(void)
{
    sdword index;

    for (index=0; index < kbTOTAL_COMMANDS; index++)
    {
        kbKeySaveTable[index] = kbKeyTable[index];
        kbKeySavedKeys[index] = kbKeyTable[index].primarykey;
    }
}

/*-----------------------------------------------------------------------------
    Name        : kbKeyUsed
    Description : This functino returns true if the key is already bound to something, FALSE otherwise
    Inputs      : key to check
    Outputs     : boolean
    Parameters  : sdword keytocheck
    Return      : bool32
-----------------------------------------------------------------------------*/
bool32 kbKeyUsed(udword keytocheck)
{
    sdword index, count=0;

    for (index=0; index < kbTOTAL_COMMANDS; index++)
    {
        if (kbKeyTable[index].primarykey == keytocheck)
        {
            count++;
        }
    }

    if (count > 1) return(TRUE);

    return(FALSE);
}

/*-----------------------------------------------------------------------------
    Name        : kbCommandKeyIsHit
    Description : This function will return true if the key is hit for the command specified
    Inputs      : command
    Outputs     : boolean
    Parameters  : udword command
    Return      : bool32
-----------------------------------------------------------------------------*/
bool32 kbCommandKeyIsHit(udword command)
{
    return(keyIsHit(kbKeyTable[command].primarykey));
}

/*-----------------------------------------------------------------------------
    Name        : kbKeyBoundToCommand
    Description : This function returns the key that is bound to that command
    Inputs      : Command to get the key for
    Outputs     : key bound
    Parameters  : udword command
    Return      : udword
-----------------------------------------------------------------------------*/
udword kbKeyBoundToCommand(udword command)
{
    if (kbKeyTable != NULL) return(kbKeyTable[command].primarykey);

    return (0);
}

/*-----------------------------------------------------------------------------
    Name        : kbInitKeyBindings
    Description : This function Initializes the keybindings.
    Inputs      : void
    Outputs     : none
    Parameters  : void
    Return      : void
-----------------------------------------------------------------------------*/
void kbInitKeyBindings(void)
{
    sdword index;
    bool32   bInFile=FALSE;

    kbKeyListFont = frFontRegister("hw_eurosecond_11.hff");

    for (index=0;index<KEY_TOTAL_KEYS;index++)
    {
        kbCanMapKey[index] = FALSE;
    }

    if (!bkDisableKeyRemap)
    {
        kbCanMapKey[TABKEY]         = TRUE;
        kbCanMapKey[BKEY]           = TRUE;
        kbCanMapKey[CKEY]           = TRUE;
        kbCanMapKey[VKEY]           = TRUE;
        kbCanMapKey[DKEY]           = TRUE;
        kbCanMapKey[EKEY]           = TRUE;
        kbCanMapKey[FKEY]           = TRUE;
        kbCanMapKey[RKEY]           = TRUE;
        kbCanMapKey[HKEY2]           = TRUE;
        kbCanMapKey[MKEY]           = TRUE;
        kbCanMapKey[RBRACK]         = TRUE;
        kbCanMapKey[LBRACK]         = TRUE;
        kbCanMapKey[SKEY]           = TRUE;
        kbCanMapKey[ZKEY]           = TRUE;
        kbCanMapKey[CAPSLOCKKEY]    = TRUE;
//        kbCanMapKey[SPACEKEY]       = TRUE;
        kbCanMapKey[HOMEKEY]        = TRUE;
        kbCanMapKey[KKEY]           = TRUE;
        kbCanMapKey[TILDEKEY]       = TRUE;
        kbCanMapKey[LKEY]           = TRUE;
        // start of extra keys
//        kbCanMapKey[PRINTKEY]       = TRUE;
        kbCanMapKey[ARRUP]          = TRUE;
        kbCanMapKey[ARRDOWN]        = TRUE;
        kbCanMapKey[ENDKEY]         = TRUE;
        kbCanMapKey[INSERTKEY]      = TRUE;
        kbCanMapKey[DELETEKEY]      = TRUE;
//        kbCanMapKey[LESSTHAN]       = TRUE;
//        kbCanMapKey[GREATERTHAN]    = TRUE;
        kbCanMapKey[AKEY]           = TRUE;
        kbCanMapKey[NKEY]           = TRUE;
        kbCanMapKey[OKEY]           = TRUE;

        if (pilotView)
        {
            kbCanMapKey[QKEY]           = FALSE;
        }
        else
        {
            kbCanMapKey[QKEY]           = TRUE;
        }

        kbCanMapKey[UKEY]           = TRUE;
        kbCanMapKey[WKEY]           = TRUE;
        kbCanMapKey[XKEY]           = TRUE;
        kbCanMapKey[YKEY]           = TRUE;
        kbCanMapKey[NUMPAD0]        = TRUE;
        kbCanMapKey[NUMPAD1]        = TRUE;
        kbCanMapKey[NUMPAD2]        = TRUE;
        kbCanMapKey[NUMPAD3]        = TRUE;
        kbCanMapKey[NUMPAD4]        = TRUE;
        kbCanMapKey[NUMPAD5]        = TRUE;
        kbCanMapKey[NUMPAD6]        = TRUE;
        kbCanMapKey[NUMPAD7]        = TRUE;
        kbCanMapKey[NUMPAD8]        = TRUE;
        kbCanMapKey[NUMPAD9]        = TRUE;
        kbCanMapKey[NUMSTARKEY]     = TRUE;
        kbCanMapKey[NUMSLASHKEY]    = TRUE;
        kbCanMapKey[NUMDOTKEY]      = TRUE;
 //       kbCanMapKey[BACKSLASHKEY]   = TRUE;
    }

    kbKeyToString[AKEY] =         strAKEY;
    kbKeyToString[BKEY] =         strBKEY;
    kbKeyToString[CKEY] =         strCKEY;
    kbKeyToString[DKEY] =         strDKEY;
    kbKeyToString[EKEY] =         strEKEY;
    kbKeyToString[FKEY] =         strFKEY;
    kbKeyToString[GKEY] =         strGKEY;
    kbKeyToString[HKEY2] =         strHKEY;
    kbKeyToString[IKEY] =         strIKEY;
    kbKeyToString[JKEY] =         strJKEY;
    kbKeyToString[KKEY] =         strKKEY;
    kbKeyToString[LKEY] =         strLKEY;
    kbKeyToString[MKEY] =         strMKEY;
    kbKeyToString[NKEY] =         strNKEY;
    kbKeyToString[OKEY] =         strOKEY;
    kbKeyToString[PKEY] =         strPKEY;
    kbKeyToString[QKEY] =         strQKEY;
    kbKeyToString[RKEY] =         strRKEY;
    kbKeyToString[SKEY] =         strSKEY;
    kbKeyToString[TKEY] =         strTKEY;
    kbKeyToString[UKEY] =         strUKEY;
    kbKeyToString[VKEY] =         strVKEY;
    kbKeyToString[WKEY] =         strWKEY;
    kbKeyToString[XKEY] =         strXKEY;
    kbKeyToString[YKEY] =         strYKEY;
    kbKeyToString[ZKEY] =         strZKEY;
    kbKeyToString[BACKSPACEKEY] = strBACKSPACEKEY;
    kbKeyToString[TABKEY] =       strTABKEY;
    kbKeyToString[ARRLEFT] =      strARRLEFT;
    kbKeyToString[ARRRIGHT] =     strARRRIGHT;
    kbKeyToString[ARRUP] =        strARRUP;
    kbKeyToString[ARRDOWN] =      strARRDOWN;
    kbKeyToString[ENDKEY] =       strENDKEY;
    kbKeyToString[LBRACK] =       strLBRACK;
    kbKeyToString[RBRACK] =       strRBRACK;
    kbKeyToString[CAPSLOCKKEY] =  strCAPSLOCKKEY;
    kbKeyToString[SPACEKEY] =     strSPACEKEY;
    kbKeyToString[ENTERKEY] =     strENTERKEY;
    kbKeyToString[HOMEKEY] =      strHOMEKEY;
    kbKeyToString[PAGEDOWNKEY] =  strPAGEDOWNKEY;
    kbKeyToString[PAGEUPKEY] =    strPAGEUPKEY;
    kbKeyToString[BACKSLASHKEY] = strBACKSLASHKEY;
    kbKeyToString[PAUSEKEY] =     strPAUSEKEY;
    kbKeyToString[SCROLLKEY] =    strSCROLLKEY;
    kbKeyToString[PRINTKEY] =     strPRINTKEY;
    kbKeyToString[INSERTKEY] =    strINSERTKEY;
    kbKeyToString[DELETEKEY] =    strDELETEKEY;
    kbKeyToString[LESSTHAN] =     strLESSTHAN;
    kbKeyToString[GREATERTHAN] =  strGREATERTHAN;
    kbKeyToString[TILDEKEY] =     strTILDEKEY;
    kbKeyToString[NUMPAD0] =      strNUMPAD0;
    kbKeyToString[NUMPAD1] =      strNUMPAD1;
    kbKeyToString[NUMPAD2] =      strNUMPAD2;
    kbKeyToString[NUMPAD3] =      strNUMPAD3;
    kbKeyToString[NUMPAD4] =      strNUMPAD4;
    kbKeyToString[NUMPAD5] =      strNUMPAD5;
    kbKeyToString[NUMPAD6] =      strNUMPAD6;
    kbKeyToString[NUMPAD7] =      strNUMPAD7;
    kbKeyToString[NUMPAD8] =      strNUMPAD8;
    kbKeyToString[NUMPAD9] =      strNUMPAD9;
    kbKeyToString[NUMMINUSKEY] =  strNUMMINUSKEY;
    kbKeyToString[NUMPLUSKEY] =   strNUMPLUSKEY;
    kbKeyToString[NUMSTARKEY] =   strNUMSTARKEY;
    kbKeyToString[NUMSLASHKEY] =  strNUMSLASHKEY;
    kbKeyToString[NUMDOTKEY] =    strNUMDOTKEY;
    kbKeyToString[MINUSKEY] =     strMINUSKEY;
    kbKeyToString[PLUSKEY] =      strPLUSKEY;
    kbKeyToString[F1KEY] =        strF1KEY;
    kbKeyToString[F2KEY] =        strF2KEY;
    kbKeyToString[F3KEY] =        strF3KEY;
    kbKeyToString[F4KEY] =        strF4KEY;
    kbKeyToString[F5KEY] =        strF5KEY;
    kbKeyToString[F6KEY] =        strF6KEY;
    kbKeyToString[F7KEY] =        strF7KEY;
    kbKeyToString[F8KEY] =        strF8KEY;
    kbKeyToString[F9KEY] =        strF9KEY;
    kbKeyToString[F10KEY] =       strF10KEY;
    kbKeyToString[F11KEY] =       strF11KEY;
    kbKeyToString[F12KEY] =       strF12KEY;

    for (index=0;index<kbTOTAL_COMMANDS;index++)
    {
        if (kbKeySavedKeys[index] != 0) bInFile = TRUE;
    }

    switch (strCurLanguage)
    {
        case languageEnglish:
            kbKeyTable = &kbKeyTableEnglish[0];
        break;
        case languageFrench:
            kbKeyTable = &kbKeyTableFrench[0];
        break;
        case languageGerman:
            kbKeyTable = &kbKeyTableGerman[0];
        break;
        case languageSpanish:
            kbKeyTable = &kbKeyTableSpanish[0];
        break;
        case languageItalian:
            kbKeyTable = &kbKeyTableItalian[0];
        break;
    }

    if (bInFile)
    {
        for (index=0;index<kbTOTAL_COMMANDS;index++)
        {
            kbKeyTable[index].primarykey = kbKeySavedKeys[index];
        }
    }

    // check the key-bindings and reset anything that's messed up
    for (index=0;index<kbTOTAL_COMMANDS;index++)
    {
        if (kbKeyTable[index].primarykey == 0)
        {
            // i guess the user can have no key for a function ?? don't know why but ...
            kbKeyTable[index].status = KB_NotBound;
            continue;
        }
        // is the key even a mappable one ?
        if (!kbCanMapKey[kbKeyTable[index].primarykey])
        {
            // just assign it to default key
            kbKeyTable[index].primarykey = kbKeyTable[index].defaultkey;
            kbKeyTable[index].status = KB_NormalBound;
        }
        // make sure that it's only used once
        if (kbKeyUsed(kbKeyTable[index].primarykey))
        {   // set to not mapped, can't have the same key do multiple things
            kbKeyTable[index].primarykey = 0;
            kbKeyTable[index].status = KB_NotBound;
        }
    }
}

/*=============================================================================
    CAT (M2-R6 mainrgn-popup): Cataclysm's key commands. The exe's table 0x008b6310 gives each command (the rows of
    default_keys.cfg, 1 = Move .. 0x96) two callbacks, both called with the sensors-manager state
    (cat_kbCommandExecute 0x0054eb10); the popup menu, the overlay's buttons and the key bindings run commands
    through it. This port fills the rows the popup menu and the main region reach (their callbacks are the exe's
    small functions at 0x0054ecc0 .. 0x005507xx, which the export does not list; named here by their commands).
    CAT-TODO(M2-R6 key bindings): the other rows, the bindings (default_keys.cfg), cat_kbCommandKeyString's key
    names and cat_kbCommandKeyMatch's binding test (key-binding port)
=============================================================================*/
#include "CameraCommand.h"
#include "CommandWrap.h"
#include "InfoOverlay.h"
#include "mainrgn.h"
#include "PopupMenu.h"
#include "Select.h"
#include "Sensors.h"
#include "ShipPrint.h"
#include "ShipSelect.h"
#include "SoundEvent.h"
#include "SpeechEvent.h"
#include "Tutor.h"
#include "Universe.h"

typedef void (*CatKbCommandCB)(udword state);

#define CAT_KB_NUMBER_COMMANDS      0x97
// the Pause command's flag 0x00a6ed74 (its callback 0x00550640; LoadUniverse, univUpdateReset and gameEnd clear it).
//      CAT-TODO(M2-R6 key bindings): the Pause command; until then the game is never paused by it
#define CAT_KB_PAUSED()             FALSE

// the callbacks (each the exe's function of that row)
static void catKbMove(udword state)             { (void)state; cat_mrMoveShips(); }                  // 0x0054ecc0
static void catKbDock(udword state)             { (void)state; cat_mrDockShips(); }                  // 0x0054ecd0
static void catKbRetire(udword state)           { (void)state; mrRetire(NULL, NULL); }               // 0x0054ece0
static void catKbCancelOrders(udword state)                                                          // 0x0054ecf0
{
    (void)state;
    if ((tutorial != TUTORIAL_ONLY) || ((*(udword *)&tutEnable & 0x8000) != 0))
    {
        cat_uiHaltSelection();
        soundEventPlay(NULL, 0x108, NULL);
    }
}
static void catKbScuttle(udword state)          { (void)state; mrScuttle(NULL, NULL); }              // 0x0054ed30
static void catKbKamikaze(udword state)                                                              // 0x0054ed40
{
    sdword index;
    CommandToDo *command;

    (void)state;
    if (!MakeSelectionKamikazeCapable((SelectCommand *)&selSelected))
    {
        return;
    }
    for (index = 0; index < selSelected.numShips; index++)
    {                                                   // a ship already attacking or ramming answers no
        command = getShipAndItsCommand(&universe.mainCommandLayer, selSelected.ShipPtr[index]);
        if ((command != NULL) && ((command->ordertype.order == COMMAND_ATTACK) ||
                                  (command->ordertype.order == COMMAND_SPECIAL)))
        {
            break;
        }
    }
    if (index == selSelected.numShips)
    {
        if (selSelected.numShips > 0)
        {
            speechEventQueue(selSelected.ShipPtr[0], 0x4000809, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
        }
        return;
    }
    soundEventPlay(NULL, 0x108, NULL);
    speechEventQueue(selSelected.ShipPtr[0], 0x40008a7, selSelected.ShipPtr[0]->shiptype, -1, -1, -1, -1, 0.0f, -1);
    clWrapSetKamikaze(&universe.mainCommandLayer, (SelectCommand *)&selSelected);
}
static void catKbShipSpecialClear(udword state) { (void)state; mrSpecialBandBox = FALSE; }           // 0x0054ed20
static void catKbShipSpecial(udword state)                                                           // 0x0054ee20
{
    (void)state;
    if ((selSelected.numShips > 0) && !catMrSelecting && !mrSpecialBandBox)
    {                                                   // the popup menu of the special actions
        cat_popupMenuOpen(CAT_PM_TARGET_KEYCOMMAND, NULL);
    }
}
static void catKbHyperspace(udword state)       { (void)state; cat_mrHyperspace(); }                 // 0x005505f0
static void catKbWaypoints(udword state)        { (void)state; cat_mrWaypointsToggle(); }            // 0x0054ef60
static void catKbWaypointModes(udword state)    { (void)state; cat_mrWaypointModeCycle(); }          // 0x0054ef70
static void catKbWaypointNormal(udword state)   { (void)state; cat_uiSetWaypointMode(0); }           // 0x0054ef80
static void catKbWaypointEndToEnd(udword state) { (void)state; cat_uiSetWaypointMode(2); }           // 0x0054ef90
static void catKbWaypointCircular(udword state) { (void)state; cat_uiSetWaypointMode(1); }           // 0x0054efa0
static void catKbCancelLastWaypoint(udword state) { (void)state; cat_mrWaypointRemoveLast(); }       // 0x0054efb0
static void catKbUpgradeShips(udword state)     { (void)state; cat_mrUpgradeShips(); }               // 0x0054f190
static void catKbEnterSlipgate(udword state)    { (void)state; cat_mrEnterSlipgate(); }              // 0x005505e0
static void catKbHarvest(udword state)          { (void)state; mrHarvestResources(NULL, NULL); }     // 0x0054efc0
static void catKbRepair(udword state)           { (void)state; cat_uiSpecialRepair(NULL); }          // 0x0054efd0
static void catKbSalvage(udword state)          { (void)state; cat_uiSpecialSalvage(NULL); }         // 0x0054efe0
static void catKbLeechSpy(udword state)         { (void)state; cat_uiSetLeechAction(6); }            // 0x00550570
static void catKbLeechLeech(udword state)       { (void)state; cat_uiSetLeechAction(7); }            // 0x00550580
static void catKbLeechVent(udword state)        { (void)state; cat_uiSetLeechAction(8); }            // 0x00550590
static void catKbLeech(udword state)            { (void)state; cat_uiSpecialLeech(NULL); }           // 0x005505a0
static void catKbUnleech(udword state)          { (void)state; cat_uiUnleech(); }                    // 0x005505b0
static void catKbLink(udword state)             { (void)state; cat_uiLink(); }                       // 0x0054f040
static void catKbUnlink(udword state)           { (void)state; cat_uiUnlink(); }                     // 0x0054f050
static void catKbCancelMimic(udword state)      { (void)state; cat_uiUnmimic(); }                    // 0x0054f180
static void catKbForceField(udword state)       { (void)state; cat_uiProtectFormation(NULL); }       // 0x0054f120
static void catKbForceFieldUnlink(udword state) { (void)state; cat_uiProtectFormationCancel(); }     // 0x00550560
static void catKbCloak(udword state)            { (void)state; cat_uiCloakShips(); }                 // 0x0054f0b0
static void catKbCancelCloak(udword state)      { (void)state; cat_uiDecloakShips(); }               // 0x0054f0c0
static void catKbLaunchSwarmers(udword state)   { (void)state; cat_uiSpecialSwarm(NULL); }           // 0x0054f090
static void catKbCancelSwarmers(udword state)   { (void)state; cat_uiReleaseHiveSwarm(); }           // 0x0054f0a0
static void catKbBigGunAttack(udword state)     { (void)state; cat_uiFireSiegeCannon(); }            // 0x0054f1a0
static void catKbCancelBigGun(udword state)     { (void)state; cat_uiSiegeCannonCancel(); }          // 0x0054f1b0
static void catKbBeastAttack(udword state)      { (void)state; cat_uiSpecialInfect(NULL); }          // 0x005505c0
static void catKbEMP(udword state)              { (void)state; cat_uiSpecialEMP(NULL); }             // 0x0054f060
static void catKbRam(udword state)              { (void)state; cat_uiSpecialRam(NULL); }             // 0x005505d0
static void catKbRepulse(udword state)          { (void)state; cat_uiFireDreadnoughtCannon(); }      // 0x0054f070
static void catKbFireMissiles(udword state)     { (void)state; cat_uiSpecialFireMissile(NULL); }     // 0x0054f080
static void catKbTacticsEvasive(udword state)   { (void)state; cat_mrSetTactics(Evasive, TRUE); }    // 0x0054ee70
static void catKbTacticsNeutral(udword state)   { (void)state; cat_mrSetTactics(Neutral, TRUE); }    // 0x0054ee80
static void catKbTacticsAggressive(udword state) { (void)state; cat_mrSetTactics(Aggressive, TRUE); } // 0x0054ee90
static void catKbNextTactic(udword state)       { (void)state; cat_mrTacticsCycle(FALSE); }          // 0x0054eea0
static void catKbPrevTactic(udword state)       { (void)state; cat_mrTacticsCycle(TRUE); }           // 0x0054eeb0
static void catKbFormationDelta(udword state)   { (void)state; mrSetTheFormation(DELTA_FORMATION, TRUE, FALSE); }    // 0x0054eec0
static void catKbFormationBroad(udword state)   { (void)state; mrSetTheFormation(BROAD_FORMATION, TRUE, FALSE); }    // 0x0054eed0
static void catKbFormationDelta3D(udword state) { (void)state; mrSetTheFormation(DELTA3D_FORMATION, TRUE, FALSE); }  // 0x0054eee0
static void catKbFormationClaw(udword state)    { (void)state; mrSetTheFormation(CLAW_FORMATION, TRUE, FALSE); }     // 0x0054eef0
static void catKbFormationWall(udword state)    { (void)state; mrSetTheFormation(WALL_FORMATION, TRUE, FALSE); }     // 0x0054ef00
static void catKbFormationSphere(udword state)  { (void)state; mrSetTheFormation(SPHERE_FORMATION, TRUE, FALSE); }   // 0x0054ef10
static void catKbFormationCustom(udword state)  { (void)state; mrSetTheFormation(CUSTOM_FORMATION, TRUE, FALSE); }   // 0x0054ef20
static void catKbFormationParade(udword state)  { (void)state; mrSetTheFormation(SAME_FORMATION, TRUE, FALSE); }     // 0x0054ef30
static void catKbNextFormation(udword state)    { (void)state; cat_mrFormationCycle(FALSE); }        // 0x0054ef40
static void catKbPrevFormation(udword state)    { (void)state; cat_mrFormationCycle(TRUE); }         // 0x0054ef50
static void catKbFocus(udword state)                                                                 // cat_kbFocus 0x0054f2c0
{
    FocusCommand focus;

    if (state == 0)
    {
        if (selSelected.numShips != 0)
        {
            tutGameMessage("KB_Focus");
            soundEventPlay(NULL, 0x137, NULL);
            memcpy(&focus, &selSelected, sizeof(focus));
            ccFocus(&universe.mainCameraCommand, &focus);
        }
    }
    else if (state == 1)
    {
        soundEventPlay(NULL, 0x137, NULL);
        selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&selSelected);
        smCullAndFocusSelecting();
    }
}
static void catKbFocusOnMothership(udword state) { (void)state; cat_mrFocusOnMothership(); }        // 0x0054f350
static void catKbFocusLastShipToSpeak(udword state) { (void)state; cat_ioFocusLastMessageShip(); }  // 0x0054f360
static void catKbCancelFocus(udword state)                                                           // 0x0054f370
{
    (void)state;
    ccCancelFocus(&universe.mainCameraCommand);
    soundEventPlay(NULL, 0x10a, NULL);
}
static void catKbForwardFocus(udword state)                                                          // 0x0054f390
{
    (void)state;
    ccForwardFocus(&universe.mainCameraCommand);
    soundEventPlay(NULL, 0x109, NULL);
}
static void catKbFleetView(udword state)        { (void)state; ccFocusOnFleetSmooth(&universe.mainCameraCommand); } // 0x0054f3b0
static void catKbChaseMode(udword state)                                                             // 0x0054f1c0
{
    FocusCommand focus;

    if (selSelected.numShips == 0)
    {
        bitClear(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
        return;
    }
    if (state == 0)
    {
        tutGameMessage("KB_Focus");
        soundEventPlay(NULL, 0x137, NULL);
        memcpy(&focus, &selSelected, sizeof(focus));
        ccFocus(&universe.mainCameraCommand, &focus);
    }
    else if (state == 1)
    {
        soundEventPlay(NULL, 0x137, NULL);
        selSelectionCopy((MaxAnySelection *)&selSelecting, (MaxAnySelection *)&selSelected);
        smCullAndFocusSelecting();
    }
    // CAT: the exe toggles its camera's chase bit (mainCameraCommand +0x8C bit 8, CCMODE_PILOT_SHIP here)
    if (bitTest(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP))
    {
        bitClear(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
    else
    {
        bitSet(universe.mainCameraCommand.ccMode, CCMODE_PILOT_SHIP);
    }
    tutGameMessage("Chase");
}
static void catKbSelectNone(udword state)                                                            // 0x0054f4a0
{
    (void)state;
    soundEventPlay(NULL, 0x108, NULL);
    selSelected.numShips = 0;
    ioUpdateShipTotals();
}
static void catKbSensorsManager(udword state)                                                        // cat_kbSensorsManager 0x0054f530
{
    if (state == 0)
    {
        tutGameMessage("KB_Sensors");
        smSensorsBegin(NULL, NULL);
    }
    else if (!smSensorsDisable)
    {
        smSensorsClose(NULL, NULL);
    }
}
static void catKbBuildManager(udword state)                                                          // 0x0054f570
{
    (void)state;
    if (!playPackets && !CAT_KB_PAUSED())
    {
        tutGameMessage("KB_Build");
        mrBuildShips(NULL, NULL);
    }
}
static void catKbResearchManager(udword state)                                                       // 0x0054f5a0
{
    (void)state;
    if (!playPackets && !CAT_KB_PAUSED())
    {
        tutGameMessage("KB_Research");
        mrResearch(NULL, NULL);
    }
}
static void catKbLaunchManager(udword state)                                                         // 0x0054f5d0
{
    (void)state;
    if (!playPackets && !CAT_KB_PAUSED())
    {
        mrLaunch(NULL, NULL);
    }
}
static void catKbTacticalDisplay(udword state)                                                       // 0x00550450
{
    (void)state;
    mrDrawTactical ^= TRUE;
    if (mrDrawTactical)
    {
        soundEventPlay(NULL, 0x11f, NULL);
        tutGameMessage("KB_TacticalDisplayOn");
    }
    else
    {
        soundEventPlay(NULL, 0x120, NULL);
        tutGameMessage("KB_TacticalDisplayOff");
    }
}
static void catKbShipInfo(udword state)                                                              // 0x00550490
{
    (void)state;
    catShipPrintOn = !catShipPrintOn;
    soundEventPlay(NULL, catShipPrintOn ? 0x138 : 0x139, NULL);
}
static void catKbOpenMenu(udword state)         { (void)state; cat_popupMenuOpen(NULL, NULL); }      // 0x0054f4c0
static void catKbEscape(udword state)           { (void)state; cat_mrEscape(); }                     // 0x0054f5f0
static void catKbNextMusicTrack(udword state)   { (void)state; musicEventNextTrack(); }              // 0x00550510 -> 0x00523c50
static void catKbPrevMusicTrack(udword state)   { (void)state; musicEventPrevTrack(); }              // 0x00550520 -> 0x00523ca0
// the sensors-view toggles (0x00550270 .. 0x005503d0)
static void catKbSVAsteroids(udword state)      { (void)state; catSmViewResources = !catSmViewResources; }
static void catKbSVFOWRanges(udword state)      { (void)state; catSmViewFOWRanges = !catSmViewFOWRanges; }
static void catKbSVAreasVisited(udword state)   { (void)state; catSmViewAreasVisited = !catSmViewAreasVisited; tutGameMessage("AreasVisitedToggle"); }
static void catKbSVAreasOccupied(udword state)  { (void)state; catSmViewAreasOccupied = !catSmViewAreasOccupied; }
static void catKbSVIcons(udword state)          { (void)state; catSmViewIcons = !catSmViewIcons; }
static void catKbSVShipMeshes(udword state)     { (void)state; catSmViewShipMeshes = !catSmViewShipMeshes; }
static void catKbSVPieplateLines(udword state)  { (void)state; catSmViewPieplateLines = !catSmViewPieplateLines; }
static void catKbSVBackground(udword state)     { (void)state; catSmViewBackground = !catSmViewBackground; }
static void catKbSVDecorativeAsteroids(udword state) { (void)state; catSmViewDecorativeAsteroids = !catSmViewDecorativeAsteroids; }
static void catKbSVCompass(udword state)        { (void)state; catSmViewCompass = !catSmViewCompass; }
static void catKbSVObjectivePointers(udword state) { (void)state; catSmViewObjectivePointers = !catSmViewObjectivePointers; }
static void catKbSVSlipgatePaths(udword state)  { (void)state; catSmViewSlipgatePaths = !catSmViewSlipgatePaths; }

// cat_kbLeechCB 0x0054eff0 / cat_kbMimicShipsCB 0x0054f0d0 / cat_kbMimicPlayersCB 0x0054f130: the popup menu opens on
//      the submenu when a selected ship can leech / mimic
static void catKbOpenSubmenu(udword upgrade, void (*open)(void))
{
    MaxSelection selection;

    cat_selSelectionCopy(&selection, (MaxSelection *)&selSelected);
    cat_selMakeShipsHaveUpgrade((SelectCommand *)&selection, upgrade);
    MakeShipsSpecialActivateCapable((SelectCommand *)&selection, TRUE);
    if (selection.numShips > 0)
    {
        open();
    }
}
void cat_kbLeechCB(udword state)                { (void)state; catKbOpenSubmenu(CAT_UPGRADE_Leech, cat_popupMenuOpenLeech); }
void cat_kbMimicShipsCB(udword state)           { (void)state; catKbOpenSubmenu(CAT_UPGRADE_Mimic, cat_popupMenuOpenMimicShips); }
void cat_kbMimicPlayersCB(udword state)         { (void)state; catKbOpenSubmenu(CAT_UPGRADE_Mimic, cat_popupMenuOpenMimicPlayers); }

// the exe's table 0x008b6310: {first callback, second callback} per command
static CatKbCommandCB catKbCommandTable[CAT_KB_NUMBER_COMMANDS][2] =
{
    [0x01] = { catKbMove, NULL },               [0x02] = { catKbDock, NULL },
    [0x03] = { catKbRetire, NULL },             [0x04] = { catKbCancelOrders, NULL },
    [0x05] = { catKbScuttle, NULL },            [0x06] = { catKbKamikaze, NULL },
    [0x07] = { catKbShipSpecialClear, catKbShipSpecial },
    [0x08] = { catKbHyperspace, NULL },         [0x09] = { catKbWaypoints, NULL },
    [0x0A] = { catKbWaypointModes, NULL },      [0x0B] = { catKbWaypointNormal, NULL },
    [0x0C] = { catKbWaypointEndToEnd, NULL },   [0x0D] = { catKbWaypointCircular, NULL },
    [0x0E] = { catKbCancelLastWaypoint, NULL }, [0x0F] = { catKbUpgradeShips, NULL },
    [0x10] = { catKbEnterSlipgate, NULL },      [0x11] = { catKbHarvest, NULL },
    [0x12] = { catKbRepair, NULL },             [0x13] = { catKbSalvage, NULL },
    [0x14] = { cat_kbLeechCB, NULL },           [0x15] = { catKbLeechSpy, NULL },
    [0x16] = { catKbLeechLeech, NULL },         [0x17] = { catKbLeechVent, NULL },
    [0x18] = { catKbLeech, NULL },              [0x19] = { catKbUnleech, NULL },
    [0x1A] = { catKbLink, NULL },               [0x1B] = { catKbUnlink, NULL },
    [0x1C] = { cat_kbMimicPlayersCB, NULL },    [0x1D] = { cat_kbMimicShipsCB, NULL },
    [0x1E] = { catKbCancelMimic, NULL },        [0x1F] = { catKbForceField, NULL },
    [0x20] = { catKbForceFieldUnlink, NULL },   [0x21] = { catKbCloak, NULL },
    [0x22] = { catKbCancelCloak, NULL },        [0x23] = { catKbLaunchSwarmers, NULL },
    [0x24] = { catKbCancelSwarmers, NULL },     [0x25] = { catKbBigGunAttack, NULL },
    [0x26] = { catKbCancelBigGun, NULL },       [0x27] = { catKbBeastAttack, NULL },
    [0x28] = { catKbEMP, NULL },                [0x29] = { catKbRam, NULL },
    [0x2A] = { catKbRepulse, NULL },            [0x2B] = { catKbFireMissiles, NULL },
    [0x2C] = { catKbTacticsEvasive, NULL },     [0x2D] = { catKbTacticsNeutral, NULL },
    [0x2E] = { catKbTacticsAggressive, NULL },  [0x2F] = { catKbNextTactic, NULL },
    [0x30] = { catKbPrevTactic, NULL },         [0x31] = { catKbFormationDelta, NULL },
    [0x32] = { catKbFormationBroad, NULL },     [0x33] = { catKbFormationDelta3D, NULL },
    [0x34] = { catKbFormationClaw, NULL },      [0x35] = { catKbFormationWall, NULL },
    [0x36] = { catKbFormationSphere, NULL },    [0x37] = { catKbFormationCustom, NULL },
    [0x38] = { catKbFormationParade, NULL },    [0x39] = { catKbNextFormation, NULL },
    [0x3A] = { catKbPrevFormation, NULL },      [0x3B] = { catKbFocus, NULL },
    [0x3C] = { catKbFocusOnMothership, NULL },  [0x3D] = { catKbFocusLastShipToSpeak, NULL },
    [0x3E] = { catKbCancelFocus, NULL },        [0x3F] = { catKbForwardFocus, NULL },
    [0x40] = { catKbFleetView, NULL },          [0x47] = { catKbChaseMode, NULL },
    [0x49] = { catKbSelectNone, NULL },         [0x4F] = { catKbSensorsManager, NULL },
    [0x50] = { catKbBuildManager, NULL },       [0x51] = { catKbResearchManager, NULL },
    [0x52] = { catKbLaunchManager, NULL },      [0x53] = { catKbTacticalDisplay, NULL },
    [0x54] = { catKbShipInfo, NULL },           [0x56] = { catKbOpenMenu, NULL },
    [0x5A] = { catKbEscape, NULL },             [0x60] = { catKbNextMusicTrack, NULL },
    [0x61] = { catKbPrevMusicTrack, NULL },
    [0x8B] = { catKbSVAsteroids, NULL },        [0x8C] = { catKbSVFOWRanges, NULL },
    [0x8D] = { catKbSVAreasVisited, NULL },     [0x8E] = { catKbSVAreasOccupied, NULL },
    [0x8F] = { catKbSVIcons, NULL },            [0x90] = { catKbSVShipMeshes, NULL },
    [0x91] = { catKbSVPieplateLines, NULL },    [0x92] = { catKbSVBackground, NULL },
    [0x93] = { catKbSVDecorativeAsteroids, NULL }, [0x94] = { catKbSVCompass, NULL },
    [0x95] = { catKbSVObjectivePointers, NULL },   [0x96] = { catKbSVSlipgatePaths, NULL },
};

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandExecute                              0x0054eb10
    Description : Runs a key command's two callbacks with the sensors-manager state.
----------------------------------------------------------------------------*/
void cat_kbCommandExecute(udword state, sdword command)
{
    if ((command < 0) || (command >= CAT_KB_NUMBER_COMMANDS))
    {                                                   // CAT-FIX: the exe indexes its table unchecked
        return;
    }
    if (catKbCommandTable[command][0] != NULL)
    {
        catKbCommandTable[command][0](state);
    }
    if (catKbCommandTable[command][1] != NULL)
    {
        catKbCommandTable[command][1](state);
    }
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandKeyString                            0x0054e960
    Description : The names of a command's keys ("No key binding" for command 0). CAT-TODO(M2-R6 key bindings): the
                  bindings are not loaded yet, so a command's list is empty (as the exe's for an unbound command)
----------------------------------------------------------------------------*/
void cat_kbCommandKeyString(sdword command, char *out)
{
    if (command != 0)
    {
        out[0] = '\0';
        return;
    }
    strcpy(out, strGetString(str_UI_NoKeyBinding));
}

/*-----------------------------------------------------------------------------
    Name        : cat_kbCommandKeyMatch                             0x0054ebc0
    Description : Is 'key' (with the modifiers) bound to the command; execute runs it.
                  CAT-TODO(M2-R6 key bindings): no bindings are loaded yet, so no key matches
----------------------------------------------------------------------------*/
bool32 cat_kbCommandKeyMatch(udword state, sdword command, udword key, bool32 execute)
{
    (void)state;
    (void)command;
    (void)key;
    (void)execute;
    return FALSE;
}
