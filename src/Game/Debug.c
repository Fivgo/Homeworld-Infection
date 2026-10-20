// =============================================================================
//  Debug.c
// =============================================================================
//  Copyright Relic Entertainment, Inc. All rights reserved.
//  Created June 1997 by Luke Moloney
// =============================================================================

#include "Debug.h"

#include "File.h"
#include "utility.h"

#ifdef __GNUC__
    #ifndef _STDARG_H
        #include <stdarg.h> 
    #endif
#endif

#if !defined(_WIN32) && !defined(__SANITIZE_ADDRESS__)
    #include <signal.h>                                     // HWI-FIX: dbgFatalSignalsInstall
    #include <unistd.h>
#endif

#if defined _MSC_VER
	#define  snprintf   _snprintf
	#define vsnprintf  _vsnprintf
#endif


#define DBG_ExitCode            0xfed5   // default exit code

/*=============================================================================
    Data:
=============================================================================*/
char dbgFatalErrorString[DBG_BufferLength];
sdword dbgAllowInterrupts = TRUE;

/*-----------------------------------------------------------------------------
    Name        : dbgStackDump
    Description : Dumps the stack to a file for debugging purposes.
    Inputs      : void
    Outputs     :
    Return      : filename of file written to, or NULL on error
----------------------------------------------------------------------------*/
#if DBG_STACK_CONTEXT
static char dbgStackFilename[PATH_MAX + 1];
udword dbgStackBase = 0;


char *dbgStackDump(void)
{
    udword nDwords, _ESP;
    char *blankPtr;
    struct tm *newtime;
    time_t aclock;
    FILE *fp;
    udword referenceAddress;

    if (dbgStackBase == 0)
    {
        return(NULL);
    }

    _ESP = _ESP & (~3);                                     //round off to dword boundary
    dbgStackBase = dbgStackBase & (~3);
    nDwords = dbgStackBase - _ESP;

    //create a filename with the time in it
    time( &aclock );                                        //Get time in seconds
    newtime = localtime( &aclock );                         //Convert time to struct
    sprintf(dbgStackFilename, "stack-%s.dump", asctime(newtime));
    for (blankPtr = dbgStackFilename; *blankPtr; blankPtr++)
    {
        if (strchr(": \n\r", *blankPtr))
        {
            *blankPtr = '-';
        }
    }

    blankPtr = filePathPrepend(dbgStackFilename, FF_UserSettingsPath);
    if (!fileMakeDestinationDirectory(blankPtr))
        return NULL;

    fp = fopen(blankPtr, "wb");                             //open the dump file
    if (fp == NULL)
    {
        return(NULL);
    }
    fwrite(&_ESP, sizeof(udword), 1, fp);                   //write the stack reference
    referenceAddress = (udword)&dbgFatalf;
    fwrite(&referenceAddress, sizeof(udword), 1, fp);       //write the .text reference
    while (nDwords)
    {
        fwrite((udword *)_ESP, sizeof(udword), 1, fp);      //write the stack block
        nDwords--;
        _ESP++;
    }
    fclose(fp);                                             //close the file
    return(dbgStackFilename);
}
#endif //DBG_STACK_CONTEXT

/*-----------------------------------------------------------------------------
    Name        : dbgMessage
    Description : Post a generic message
    Inputs      : string - message to print
    Outputs     : ..
    Return      : void
----------------------------------------------------------------------------*/
void dbgMessage(char *string)
{
    /* Debug window disabled (using stdout instead, at least for now).
       dbw*() functions in other parts of code (main.c, utility.c, and
       Options.c) will need to be uncommented if the debug window is
       reenabled. */
    /*return(dbwPrint(0, string));*/
    printf("%s\n", string);
    fflush(NULL);
}

/*-----------------------------------------------------------------------------
 Name        : dbgMessagef
 Description : same as dbgMessage() but with interpolated string
 Inputs      : same as dbgMessage()
 Outputs     :
 Return      : void
 ----------------------------------------------------------------------------*/
void dbgMessagef(char *format, ...)
{
    char message[DBG_BufferLength];
    va_list argList;

    // interpolate variable arguments into formatted string
    va_start(argList, format);
    vsnprintf(message, DBG_BufferMax, format, argList);
    va_end(argList);

    dbgMessage(message);
}

/*-----------------------------------------------------------------------------
    Name        : dbgWarning
    Description : Post a warning message
    Inputs      :
        file - points to file name string
        line - line number called from
        string - output string
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void dbgWarning(char *file, sdword line, char *string)
{
    snprintf(dbgFatalErrorString, DBG_BufferMax, "\n%s (%d): Warning - %s", file, line, string);

    dbgMessage(dbgFatalErrorString);
}

/*-----------------------------------------------------------------------------
 Name        : dbgWarningf
 Description : same as dbgWarning() but with interpolated string
 Inputs      : same as dbgWarning()
 Outputs     :
 Return      : void
 ----------------------------------------------------------------------------*/
void dbgWarningf(char *file, sdword line, char *format, ...)
{
    char message[DBG_BufferLength];
    va_list argList;

    // interpolate variable arguments into formatted string
    va_start(argList, format);
    vsnprintf(message, DBG_BufferMax, format, argList);
    va_end(argList);

    dbgWarning(file, line, message);
}

/*-----------------------------------------------------------------------------
    Name        : dbgFatal
    Description : Print a fatal error message and exit program.
    Inputs      :
        file - points to file name string
        line - line number called from
        string - output string
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
#if defined(__SANITIZE_ADDRESS__)
void __sanitizer_print_stack_trace(void);         // HWI-FIX: <sanitizer/common_interface_defs.h>
#endif

void dbgFatal(char *file, sdword line, char *string)
{
    char *fileName = NULL;

#if DBG_STACK_CONTEXT
    fileName = dbgStackDump();
#endif

    if (fileName) {
        snprintf(dbgFatalErrorString, DBG_BufferMax, "\n%s (%d): Fatal error - %s (Dumped to '%s')", file, line, string, fileName);
    }
    else {
        snprintf(dbgFatalErrorString, DBG_BufferMax, "\n%s (%d): Fatal error - %s", file, line, string);
    }

    // HWI-FIX: the fatal string goes to stderr (unbuffered), after everything already printed, before the deliberate
    //      fault or the exit, in every build: GoK's SDL utyFatalErrorWaitLoop has no message box to show it
    fflush(stdout);
    fprintf(stderr, "%s\n", dbgFatalErrorString);
#if defined(__SANITIZE_ADDRESS__)
    // HWI-FIX (M2-R9 missions-02-09): the debug (ASan) build also prints the fatal error's call stack; the harness
    //      runs with /nodebugInt, so no deliberate fault follows to show where it came from
    __sanitizer_print_stack_trace();
#endif
    fflush(stderr);

    if (dbgAllowInterrupts)
    {
        char *null_ptr = NULL;
        *null_ptr = 1;  // deliberate out of bounds memory assignment
    }

    utyFatalErrorWaitLoop(DBG_ExitCode);                    //exit with a MessageBox
}

/*-----------------------------------------------------------------------------
 Name        : dbgFatalf
 Description : same as dbgFatal() but with interpolated string
 Inputs      : same as dbgFatal()
 Outputs     :
 Return      : void
 ----------------------------------------------------------------------------*/
void dbgFatalf(char *file, sdword line, char *format, ...)
{
    char message[DBG_BufferLength];
    va_list argList;

    // interpolate variable arguments into formatted string
    va_start(argList, format);
    vsnprintf(message, DBG_BufferMax, format, argList);
    va_end(argList);
    
    dbgFatal(file, line, message);
}

/*-----------------------------------------------------------------------------
    Name        : dbgFatalSignalsInstall
    Description : HWI-FIX: a build without AddressSanitizer reports a crash signal on stderr before the process dies:
                    the release build had no handler, so a crash (or the deliberate fault of dbgFatal) ended the run
                    with no line in the log. The debug build keeps ASan's own report (no handler installed there).
    Inputs      :
    Outputs     :
    Return      :
----------------------------------------------------------------------------*/
#if !defined(_WIN32) && !defined(__SANITIZE_ADDRESS__)
static void dbgFatalSignalHandler(int sig)
{
    static const char head[] = "\nhwi-engine: fatal signal ";
    static const char tail[] = " (the engine crashed; any fatal error message is above)\n";
    char number[4];
    int n = 0;

    if (sig >= 100) number[n++] = (char)('0' + (sig / 100) % 10);
    if (sig >= 10)  number[n++] = (char)('0' + (sig / 10) % 10);
    number[n++] = (char)('0' + sig % 10);
    if (write(STDERR_FILENO, head, sizeof(head) - 1) < 0 ||
        write(STDERR_FILENO, number, (size_t)n) < 0 ||
        write(STDERR_FILENO, tail, sizeof(tail) - 1) < 0)
    {
        // nothing more can be done here
    }
    raise(sig);                                             // SA_RESETHAND: the default action ends the process as before
}

void dbgFatalSignalsInstall(void)
{
    static const int sigs[] = { SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT };
    struct sigaction action;
    size_t i;

    memset(&action, 0, sizeof(action));
    action.sa_handler = dbgFatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESETHAND | SA_NODEFER;
    for (i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++)
    {
        sigaction(sigs[i], &action, NULL);
    }
}
#else
void dbgFatalSignalsInstall(void)
{
}
#endif

/*-----------------------------------------------------------------------------
    Name        : dbgNonFatal
    Description : Non-fatal error handling: pops up a dialog but does not
                    exit the game.
    Inputs      : same as dbgFatal
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void dbgNonFatal(char *file, sdword line, char *string)
{
    snprintf(dbgFatalErrorString, DBG_BufferMax, "\n%s (%d): Non-fatal error - %s", file, line, string);

    dbgMessage(dbgFatalErrorString);

    if (dbgAllowInterrupts)
    {
        char *null_ptr = NULL;
        *null_ptr = 1;  // deliberate out of bounds memory assignment
    }

    utyNonFatalErrorWaitLoop();
}

/*-----------------------------------------------------------------------------
    Name        : dbgNonFatalf
    Description : same as dbgNonFatal() but with interpolated string
    Inputs      : same as dbgNonFatal()
    Outputs     :
    Return      : void
----------------------------------------------------------------------------*/
void dbgNonFatalf(char *file, sdword line, char *format, ...)
{
    char message[DBG_BufferLength];
    va_list argList;

    // interpolate variable arguments into formatted string
    va_start(argList, format);
    vsnprintf(message, DBG_BufferMax, format, argList);
    va_end(argList);

    dbgNonFatal(file, line, message);
}
