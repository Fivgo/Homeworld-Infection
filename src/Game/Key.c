
#include <stdio.h>
#include <string.h>

#include "Debug.h"
#include "Demo.h"
#include "Task.h"
#include "Key.h"
#include "Options.h"
#include "ConsMgr.h"
#include "StringsOnly.h"

/*=============================================================================
    Data:
=============================================================================*/
volatile keyScanType keyScanCode[KEY_TOTAL_KEYS];
volatile keyScanType keySaveScan[KEY_TOTAL_KEYS];
real32 keyLastTimeHit = 0.0f;

//key repeat buffer
ubyte keyNumberBufferKeys = 0;
keybufferchar keyBuffer[KEY_BufferLength];

uword keysToNotBuffer[] = {
    SHIFTKEY, LMOUSE_BUTTON, RMOUSE_BUTTON, MMOUSE_BUTTON,
    FLYWHEEL_UP, FLYWHEEL_DOWN, LMOUSE_DOUBLE, RMOUSE_DOUBLE,
    MMOUSE_DOUBLE,
    0};
/*=============================================================================
    Functions:
=============================================================================*/
/*-----------------------------------------------------------------------------
    Name        : keyInit
    Description : Start the keyboard module
    Inputs      : void
    Outputs     : clears the keyScanCode array
    Return      : void
----------------------------------------------------------------------------*/
void keyInit(void)
{
    keyClearAll();
    cat_keyNameStringsInit();                               // CAT: keyInit 0x0049ae00 ends with the key-name strings
}

/*=============================================================================
    CAT (M2-R6 startup-keys): Cataclysm's key names and its key ids.

    The exe's key ids are Win32 virtual-key codes: the numbers of default_keys.cfg / keys.cfg, the ids of its region
    key events and its keyScanCode index. HWI: this tree's key index is an SDL scancode (GoK); the key bindings keep the
    exe's virtual-key codes (KeyBindings.c) and catKeyVkFromKey / catKeyVkIsHit convert. The table names every key the
    exe's key-name table (cat_keyNameStringsInit) and its keyboard tables (cat_kbKeyToString) know, plus HW1's mouse ids
    (Key.h of Relic's source: LMOUSE_BUTTON 1 .. MMOUSE_DOUBLE 0xf8).
=============================================================================*/
typedef struct
{
    udword vk;                                              // the Win32 virtual-key code
    keyindex key;                                           // this tree's key index
}
CatKeyVk;

static const CatKeyVk catKeyVkTable[] =
{
    {0x01, LMOUSE_BUTTON},  {0x02, RMOUSE_BUTTON},  {0x03, FLYWHEEL_UP},    {0x04, MMOUSE_BUTTON},
    {0x05, FLYWHEEL_DOWN},  {0x06, LMOUSE_DOUBLE},  {0x07, RMOUSE_DOUBLE},  {0xf8, MMOUSE_DOUBLE},
    {0x08, BACKSPACEKEY},   {0x09, TABKEY},         {0x0d, RETURNKEY},      {0x0d, ENTERKEY},
    {0x10, LSHIFTKEY},      {0x10, RSHIFTKEY},      {0x11, LCONTROLKEY},    {0x11, RCONTROLKEY},
    {0x12, LALTKEY},        {0x12, RALTKEY},        {0x13, PAUSEKEY},       {0x14, CAPSLOCKKEY},
    {0x1b, ESCKEY},         {0x20, SPACEKEY},       {0x21, PAGEUPKEY},      {0x22, PAGEDOWNKEY},
    {0x23, ENDKEY},         {0x24, HOMEKEY},        {0x25, ARRLEFT},        {0x26, ARRUP},
    {0x27, ARRRIGHT},       {0x28, ARRDOWN},        {0x2a, PRINTKEY},       {0x2c, PRINTKEY},
    {0x2d, INSERTKEY},      {0x2e, DELETEKEY},
    {0x30, ZEROKEY},  {0x31, ONEKEY},   {0x32, TWOKEY},   {0x33, THREEKEY}, {0x34, FOURKEY},
    {0x35, FIVEKEY},  {0x36, SIXKEY},   {0x37, SEVENKEY}, {0x38, EIGHTKEY}, {0x39, NINEKEY},
    {0x41, AKEY}, {0x42, BKEY}, {0x43, CKEY}, {0x44, DKEY}, {0x45, EKEY}, {0x46, FKEY}, {0x47, GKEY},
    {0x48, HKEY2}, {0x49, IKEY}, {0x4a, JKEY}, {0x4b, KKEY}, {0x4c, LKEY}, {0x4d, MKEY}, {0x4e, NKEY},
    {0x4f, OKEY}, {0x50, PKEY}, {0x51, QKEY}, {0x52, RKEY}, {0x53, SKEY}, {0x54, TKEY}, {0x55, UKEY},
    {0x56, VKEY}, {0x57, WKEY}, {0x58, XKEY}, {0x59, YKEY}, {0x5a, ZKEY},
    {0x5b, LMETAKEY},       {0x5c, RMETAKEY},
    {0x60, NUMPAD0}, {0x61, NUMPAD1}, {0x62, NUMPAD2}, {0x63, NUMPAD3}, {0x64, NUMPAD4},
    {0x65, NUMPAD5}, {0x66, NUMPAD6}, {0x67, NUMPAD7}, {0x68, NUMPAD8}, {0x69, NUMPAD9},
    {0x6a, NUMSTARKEY},     {0x6b, NUMPLUSKEY},     {0x6d, NUMMINUSKEY},    {0x6e, NUMDOTKEY},
    {0x6f, NUMSLASHKEY},
    {0x70, F1KEY}, {0x71, F2KEY}, {0x72, F3KEY}, {0x73, F4KEY}, {0x74, F5KEY},  {0x75, F6KEY},
    {0x76, F7KEY}, {0x77, F8KEY}, {0x78, F9KEY}, {0x79, F10KEY}, {0x7a, F11KEY}, {0x7b, F12KEY},
    {0x90, SDL_SCANCODE_NUMLOCKCLEAR},              {0x91, SCROLLKEY},
    {0xba, SDL_SCANCODE_SEMICOLON},                 {0xbb, PLUSKEY},        {0xbc, LESSTHAN},
    {0xbd, MINUSKEY},       {0xbe, GREATERTHAN},    {0xbf, SDL_SCANCODE_SLASH},
    {0xc0, TILDEKEY},       {0xdb, LBRACK},         {0xdc, BACKSLASHKEY},   {0xdd, RBRACK},
    {0xde, SDL_SCANCODE_APOSTROPHE},
};
#define CAT_KEY_VK_ENTRIES      (sizeof(catKeyVkTable) / sizeof(catKeyVkTable[0]))

/*-----------------------------------------------------------------------------
    Name        : catKeyVkFromKey
    Description : HWI: the exe's virtual-key code of one of this tree's key indices (0: no such key in the exe)
----------------------------------------------------------------------------*/
udword catKeyVkFromKey(keyindex key)
{
    udword index;

    key = leftModSide(key);
    for (index = 0; index < CAT_KEY_VK_ENTRIES; index++)
    {
        if (catKeyVkTable[index].key == key)
        {
            return catKeyVkTable[index].vk;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : catKeyFromVk
    Description : HWI: this tree's (first) key index of an exe virtual-key code (0: none)
----------------------------------------------------------------------------*/
keyindex catKeyFromVk(udword vk)
{
    udword index;

    for (index = 0; index < CAT_KEY_VK_ENTRIES; index++)
    {
        if (catKeyVkTable[index].vk == (vk & 0xff))
        {
            return catKeyVkTable[index].key;
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
    Name        : catKeyVkIsHit
    Description : HWI: the exe's keyScanCode[vk].keypressed: is any of this tree's keys of that virtual key down
----------------------------------------------------------------------------*/
bool32 catKeyVkIsHit(udword vk)
{
    udword index;

    for (index = 0; index < CAT_KEY_VK_ENTRIES; index++)
    {
        if ((catKeyVkTable[index].vk == (vk & 0xff)) && keyIsHit(leftModSide(catKeyVkTable[index].key)))
        {
            return TRUE;
        }
    }
    return FALSE;
}

// the exe's key-name table 0x00a44004: a string id per virtual-key code (0: none)
sdword catKeyNameStrings[256];

/*-----------------------------------------------------------------------------
    Name        : cat_keyNameStringsInit                            0x0049b1b0
    Description : The string ids of the key names (the key bindings' key-to-string for the keys without a character)
----------------------------------------------------------------------------*/
void cat_keyNameStringsInit(void)
{
    sdword index;

    for (index = 0; index < 26; index++)
    {                                                       // 'A'..'Z'
        catKeyNameStrings['A' + index] = strAKEY + index;
    }
    catKeyNameStrings[0x08] = strBACKSPACEKEY;
    catKeyNameStrings[0x09] = strTABKEY;
    catKeyNameStrings[0x25] = strARRLEFT;
    catKeyNameStrings[0x27] = strARRRIGHT;
    catKeyNameStrings[0x26] = strARRUP;
    catKeyNameStrings[0x28] = strARRDOWN;
    catKeyNameStrings[0x23] = strENDKEY;
    catKeyNameStrings[0xdb] = strLBRACK;
    catKeyNameStrings[0xdd] = strRBRACK;
    catKeyNameStrings[0x14] = strCAPSLOCKKEY;
    catKeyNameStrings[0x20] = strSPACEKEY;
    catKeyNameStrings[0xba] = strSEMICOLONKEY;
    catKeyNameStrings[0x0d] = strENTERKEY;
    catKeyNameStrings[0x24] = strHOMEKEY;
    catKeyNameStrings[0x22] = strPAGEDOWNKEY;
    catKeyNameStrings[0x21] = strPAGEUPKEY;
    catKeyNameStrings[0xdc] = strBACKSLASHKEY;
    catKeyNameStrings[0x13] = strPAUSEKEY;
    catKeyNameStrings[0x91] = strSCROLLKEY;
    catKeyNameStrings[0x2a] = strPRINTKEY;
    catKeyNameStrings[0x2d] = strINSERTKEY;
    catKeyNameStrings[0x2e] = strDELETEKEY;
    catKeyNameStrings[0xbc] = strLESSTHAN;
    catKeyNameStrings[0xbe] = strGREATERTHAN;
    catKeyNameStrings[0xc0] = strTILDEKEY;
    for (index = 0; index < 10; index++)
    {                                                       // the number pad's 0..9
        catKeyNameStrings[0x60 + index] = strNUMPAD0 + index;
    }
    catKeyNameStrings[0x6d] = strNUMMINUSKEY;
    catKeyNameStrings[0x6b] = strNUMPLUSKEY;
    catKeyNameStrings[0x6a] = strNUMSTARKEY;
    catKeyNameStrings[0x6f] = strNUMSLASHKEY;
    catKeyNameStrings[0x6e] = strNUMDOTKEY;
    catKeyNameStrings[0xbd] = strMINUSKEY;
    catKeyNameStrings[0xbb] = strPLUSKEY;
    for (index = 0; index < 12; index++)
    {                                                       // F1..F12
        catKeyNameStrings[0x70 + index] = strF1KEY + index;
    }
    catKeyNameStrings[0x10] = strSHIFTKEY;
    catKeyNameStrings[0x12] = strALTKEY;
    catKeyNameStrings[0x11] = strCTRLKEY;
    catKeyNameStrings[0x1b] = strESCKEY;
    for (index = 0; index < 10; index++)
    {                                                       // '0'..'9'
        catKeyNameStrings['0' + index] = strZEROKEY + index;
    }
    catKeyNameStrings[0xde] = strApostKEY;
}

/*-----------------------------------------------------------------------------
    Name        : keyClose
    Description : Does nothing
    Inputs      : ..
    Outputs     : ..
    Return      : ..
----------------------------------------------------------------------------*/
void keyClose(void)
{
    ;
}

/*-----------------------------------------------------------------------------
    Name        : leftModSide
    Description : Convert right shift/ctrl/alt/meta to left equiv
    Inputs      : key-index of key to set
    Outputs     : the modified key or original if unchanged
    Return      : udword
----------------------------------------------------------------------------*/
udword leftModSide(udword key){
    if (key == RSHIFTKEY)
        key = LSHIFTKEY;
    else if (key == RCONTROLKEY)
        key = LCONTROLKEY;
    else if (key == RMETAKEY) // Apple/Command/"Windows" key
        key = LMETAKEY;
    return key;
}

/*-----------------------------------------------------------------------------
    Name        : keyPressDown
    Description : Called from Windows, sets the on and sticky bits of key
    Inputs      : key-index of key to set
    Outputs     : on and sticky bit set, repeat count updated
    Return      : void
----------------------------------------------------------------------------*/
void keyPressDown(udword key)
{
	bool32 bypass = FALSE;
	bool32 shift = FALSE;
	keyScanType originalKey;

    /* Game not specific about left or right Shift/Ctrl/Alt key, so neither
       are we. */
    key = leftModSide(key);

    originalKey = keySaveScan[key];
    
#if KEY_ERROR_CHECKING
    dbgAssertOrIgnore(key < KEY_TOTAL_KEYS);
    dbgMessagef("Key pressed = %d",key);
#endif

    if (opKeyDetour)
    {
        //key redefinition in options screen

        switch (opKeyDetour)
        {
            case 1:
                opDefineThisKey((keyindex)key);
                bypass = TRUE;
                break;
            case 2:
                if (key == SHIFTKEY)
                {
                    bypass = FALSE;
                }
                else
                {
                    shift = keyIsHit(SHIFTKEY);

                    if (cmBuildHotKey((keyindex)key, shift))
                        bypass = TRUE;
                    else
                        bypass = FALSE;
                }
            //research is case 3!
        }
    }
    if (!bypass)
    {
        keyBufferAdd(key, keyIsHit(SHIFTKEY));
        keySaveScan[key].keypressed = 1;
        keySaveScan[key].keystick = 1;
        if (keySaveScan[key].keynumpressed < KEY_NUMPRESSED_MAX)
        {
            keySaveScan[key].keynumpressed++;
    #if KEY_VERBOSE_LEVEL >= 2
            dbgMessagef("keyRepeat: repeat count of 0x%x incremented to %d", key, keySaveScan[key].keynumpressed);
    #endif
        }
        if (!(demDemoRecording | demDemoPlaying))
        {
            keyScanCode[key] = keySaveScan[key];
        }
        else if (demDemoPlaying)
        {
            if (key == ESCKEY)
            {                                               //escape while playing back a demo:stop playing demo
                keySaveScan[key] = originalKey;             //don't register the keypress
                demPlayEnd();                               //stop playing the demo
            }
        }
    }
    keyLastTimeHit = taskTimeElapsed;
}

/*-----------------------------------------------------------------------------
    Name        : keyRepeat
    Description : Increment repeat count for a key
    Inputs      : key - index of key to increment repeat count of.
    Outputs     : increments repeat count, clamping at a max.  That's it.
    Return      :
----------------------------------------------------------------------------*/
void keyRepeat(udword key)
{
    /* Game not specific about left or right Shift/Ctrl/Alt key, so neither
       are we. */
    key = leftModSide(key);

#if KEY_ERROR_CHECKING
    dbgAssertOrIgnore(key < KEY_TOTAL_KEYS);
#endif
    keyBufferAdd(key, keyIsHit(SHIFTKEY));
    if (keySaveScan[key].keynumpressed < KEY_NUMPRESSED_MAX)
    {
        keySaveScan[key].keynumpressed++;
#if KEY_VERBOSE_LEVEL >= 2
        dbgMessagef("keyRepeat: repeat count of 0x%x incremented to %d", key, keySaveScan[key].keynumpressed);
#endif
    }
    if (!(demDemoRecording | demDemoPlaying))
    {
        keyScanCode[key] = keySaveScan[key];
    }
    keyLastTimeHit = taskTimeElapsed;
}

/*-----------------------------------------------------------------------------
    Name        : keyPressUp
    Description : Called from windows, clears indexed key
    Inputs      : key-index of key to clear
    Outputs     : on bit and repeat count cleared, sticky bit remains
    Return      :
----------------------------------------------------------------------------*/
void keyPressUp(udword key)
{
    /* Game not specific about left or right Shift/Ctrl/Alt key, so neither
       are we. */
    key = leftModSide(key);

    keySaveScan[key].keypressed = 0;
    keySaveScan[key].keynumpressed = 0;
#if KEY_VERBOSE_LEVEL >= 2
        dbgMessagef("keyRepeat: repeat count of 0x%x set to 0", key);
#endif
    if (!(demDemoRecording | demDemoPlaying))
    {
        keyScanCode[key] = keySaveScan[key];
    }
}

/*-----------------------------------------------------------------------------
    Name        : keyAnyKeyHit
    Description : Checks to see if any key is hit
    Inputs      : void
    Outputs     : ..
    Return      : nonzero if any key pressed
----------------------------------------------------------------------------*/
bool32 keyAnyKeyHit(void)
{
    int i;
    keyScanType keyhit = { 0,0,0 };

    for (i=0;i<KEY_TOTAL_KEYS;i++)
        keyhit.keypressed |= keyScanCode[i].keypressed;

    return (bool32)keyhit.keypressed;
}

/*-----------------------------------------------------------------------------
    Name        : keyAnyKeyStuck
    Description : Checks to see if any key is stuck, clearing the state if so.
    Inputs      : void
    Outputs     : ..
    Return      : nonzero if any key pressed
----------------------------------------------------------------------------*/
bool32 keyAnyKeyStuck(void)
{
    int i;
    keyScanType keyhit = { 0,0,0 };

    for (i=0;i<KEY_TOTAL_KEYS;i++)
    {
        keyhit.keystick |= keyScanCode[i].keystick;
        keySaveScan[i].keystick = keyScanCode[i].keystick = 0;
    }

    return (bool32)keyhit.keystick;
}

/*-----------------------------------------------------------------------------
    Name        : keyClearAll
    Description : Clear all keys.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void keyClearAll(void)
{
    memset((void *)keyScanCode,0,sizeof(keyScanCode));
    memset((void *)keySaveScan,0,sizeof(keySaveScan));
    keyBufferClear();
}

/*-----------------------------------------------------------------------------
    Name        : keyClearAllStuckKeys
    Description : Clears the stick bits from all keys that are not currently
                    pressed.
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void keyClearAllStuckKeys(void)
{
    sdword index;

    for (index = 0; index < KEY_TOTAL_KEYS; index++)
    {
        if (keyScanCode[index].keystick && (!keyScanCode[index].keypressed))
        {                                                   //if stuck but not pressed
            keyScanCode[index].keystick = 0;
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : keyBufferedKeyGet
    Description : Get a buffered keystroke for typing.
    Inputs      :
    Outputs     : bShift - was the shift key pressed when this one went down?
    Return      : ASCII key (including shift state) or 0 meaning there are no more buffered keys.
    Notes       : this function is not thread-safe
                  This function will return the shifted status of the key if
                    shift was pressed at the time this key was hit.
----------------------------------------------------------------------------*/
udword keyBufferedKeyGet(bool32 *bShift)
{
    udword keyToReturn, index;

    if (keyNumberBufferKeys == 0)
    {
        return(0);
    }
    keyToReturn = (udword)keyBuffer[0].key;
    *bShift = keyBuffer[0].bShift;
    keyScanCode[keyToReturn].keypressed = 0;
    keyScanCode[keyToReturn].keystick = 0;
    keyScanCode[keyToReturn].keynumpressed = 0;
    keySaveScan[keyToReturn].keypressed = 0;
    keySaveScan[keyToReturn].keystick = 0;
    keySaveScan[keyToReturn].keynumpressed = 0;

    for (index = 1; index < keyNumberBufferKeys; index++)
    {                                                       //shift rest of buffer back one
        keyBuffer[index - 1] = keyBuffer[index];
    }
    keyNumberBufferKeys--;
    return(keyToReturn);
}

/*-----------------------------------------------------------------------------
    Name        : keyBufferAdd
    Description : Add a keystroke to the key buffer
    Inputs      : key - scancode of key that was hit
                  bShift - was the shift key pressed?
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
void keyBufferAdd(udword key, bool32 bShift)
{
    int i;

    /* Game not specific about left or right Shift/Ctrl/Alt key, so neither
       are we. */
    key = leftModSide(key);

    for (i = 0; keysToNotBuffer[i]; i++)
    {
        if (keysToNotBuffer[i] == key)                      //don't bother storing strokes of certain special keys
            return;
    }
    if (keyNumberBufferKeys < KEY_BufferLength - 1)
    {                                                       //increase size of buffer if it's not already maxed
        keyBuffer[keyNumberBufferKeys].key = key;           //insert new key at the end
        keyBuffer[keyNumberBufferKeys].bShift = bShift;
        keyNumberBufferKeys++;
    }
}

/*-----------------------------------------------------------------------------
    Name        : keyBufferClear
    Description : Clears out the key buffer
    Inputs      : void
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void keyBufferClear(void)
{
    keyNumberBufferKeys = 0;
}

/*-----------------------------------------------------------------------------
    Name        : keyGermanToEnglish
    Description : This function converts german keyboard presses to english
    Inputs      : udword virtkey
    Outputs     : udword
    Parameters  : udword virtkey
    Return      : void
-----------------------------------------------------------------------------*/
udword keyGermanToEnglish(udword virtkey)
{
    return virtkey;
}

/*-----------------------------------------------------------------------------
    Name        : keyFrenchToEnglish
    Description : This function converts german keyboard presses to english
    Inputs      : udword virtkey
    Outputs     : udword
    Parameters  : udword virtkey
    Return      : void
-----------------------------------------------------------------------------*/
udword keyFrenchToEnglish(udword virtkey)
{
    return virtkey;
}

/*-----------------------------------------------------------------------------
    Name        : keySpanishToEnglish
    Description : This function converts german keyboard presses to english
    Inputs      : udword virtkey
    Outputs     : udword
    Parameters  : udword virtkey
    Return      : void
-----------------------------------------------------------------------------*/
udword keySpanishToEnglish(udword virtkey)
{
    return virtkey;
}

/*-----------------------------------------------------------------------------
    Name        : keyItalianToEnglish
    Description : This function converts german keyboard presses to english
    Inputs      : udword virtkey
    Outputs     : udword
    Parameters  : udword virtkey
    Return      : void
-----------------------------------------------------------------------------*/
udword keyItalianToEnglish(udword virtkey)
{
    return virtkey;
}
