/*=============================================================================
    Name    : datasetup.c
    Purpose : HWI-FEATURE(data-setup): the engine finds the game data itself (docs/features/data-setup.md).
              utyGameSystemsPreInit calls dsGameDataFind when HW_Data is not set. The order: the working directory
              if it holds cataclysm.big (the old Windows zip layout), the path saved in <settings>/gamedata-path.txt,
              an installed GOG copy, the GOG offline installer found in Downloads / Desktop / home (unpacked with
              innoextract into the per-user GameData folder), then a dialog: the GOG game folder or the offline
              installer. A GOG folder is only read. A run that cannot show a dialog never waits on one. Every step
              logs a "datasetup:" line on stdout.
=============================================================================*/

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <direct.h>
#include <shlobj.h>
#include <shobjidl.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <glob.h>
#ifndef GLOB_ONLYDIR
#define GLOB_ONLYDIR 0      // HWI-FIX(macos): a GNU extension; dsConsider checks every match for the game files anyway
#endif
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char **environ;
#endif

#include "SDL.h"

#include "Types.h"
#include "File.h"
#include "regkey.h"
#include "datasetup.h"

extern bool32 catHiddenWindow;                      // HWI-TEST: render.c, /catHiddenWindow (the progress window too)

#define DS_PATH             1024
#define DS_GOG_BIG_SIZE     76864402LL              // cataclysm.big of the GOG 1.01 hotfix (12948)
#define DS_GOG_GAME_ID      "1766413869"            // GOG's product id of Homeworld: Emergence
#define DS_GOG_GAME_ID_W    L"1766413869"
#define DS_FREE_NEEDED      (450LL * 1024 * 1024)   // the installer's files are 386 MiB
#define DS_TITLE            "Homeworld Infection: game files"
#define DS_INSTALLER_PREFIX "setup_homeworld_emergence"

#ifdef _WIN32
#define DS_SEP              "\\"
#define DS_INNOEXTRACT      "innoextract.exe"
#define dsMkdir(p)          _mkdir(p)
#define dsRmdir(p)          _rmdir(p)
#define dsGetcwd(b, n)      _getcwd(b, n)
#else
#define DS_SEP              "/"
#define DS_INNOEXTRACT      "innoextract"
#define dsMkdir(p)          mkdir(p, 0755)
#define dsRmdir(p)          rmdir(p)
#define dsGetcwd(b, n)      getcwd(b, n)
#endif

// what the engine reads from the game folder
static const char *const dsNeededFiles[] = { "cataclysm.big", "update.big", "Music4Cat.wxd", "SectSpeech.vce",
                                             "BeastSpeech.vce", "default_keys.cfg", "devstats.dat", NULL };
static const char *const dsNeededDirs[] = { "Movies", "feman", "credits", NULL };

static int dsQuit = 0;                              // the player chose Quit

// HWI-TEST: /catDataSetupScript: the answers that replace every dialog and picker
static int dsScriptOn = 0;
static int dsScriptShow = 0;                        // "show": the progress window is not hidden (a gamescope capture)
static char dsScriptBuf[4096];
static char *dsScriptPos = NULL;
static char dsScriptArg[DS_PATH];                   // the text after "key:" of the last answer

#ifdef __GNUC__
static void dsLog(const char *format, ...) __attribute__((format(printf, 1, 2)));
#endif

static void dsLog(const char *format, ...)
{
    va_list args;

    printf("datasetup: ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

/*-----------------------------------------------------------------------------
    The test script (HWI-TEST)
-----------------------------------------------------------------------------*/
void dsTestScriptSet(char *answers)
{
    char *token;

    snprintf(dsScriptBuf, sizeof(dsScriptBuf), "%s", (answers != NULL) ? answers : "");
    dsScriptPos = dsScriptBuf;
    dsScriptOn = 1;
    for (token = dsScriptBuf; token != NULL; token = strchr(token, ';'))
    {
        if (*token == ';')
        {
            token++;
        }
        if (strncmp(token, "show", 4) == 0 && (token[4] == ';' || token[4] == '\0'))
        {
            dsScriptShow = 1;
        }
    }
}

// the next scripted answer: its key ("folder") in key and its text after ':' in dsScriptArg; 0 when none is left
static int dsScriptNext(char *key, size_t keySize)
{
    while (dsScriptPos != NULL && *dsScriptPos != '\0')
    {
        char token[DS_PATH + 64];
        char *end = strchr(dsScriptPos, ';');
        size_t length = (end != NULL) ? (size_t)(end - dsScriptPos) : strlen(dsScriptPos);
        char *colon;

        if (length >= sizeof(token))
        {
            length = sizeof(token) - 1;
        }
        memcpy(token, dsScriptPos, length);
        token[length] = '\0';
        dsScriptPos = (end != NULL) ? end + 1 : dsScriptPos + strlen(dsScriptPos);
        if (token[0] == '\0' || strcmp(token, "show") == 0)
        {
            continue;
        }
        dsScriptArg[0] = '\0';
        colon = strchr(token, ':');
        if (colon != NULL)
        {
            *colon = '\0';
            snprintf(dsScriptArg, sizeof(dsScriptArg), "%s", colon + 1);
        }
        snprintf(key, keySize, "%s", token);
        return 1;
    }
    dsScriptArg[0] = '\0';
    return 0;
}

int dsQuitChosen(void)
{
    return dsQuit;
}

/*-----------------------------------------------------------------------------
    Paths and files
-----------------------------------------------------------------------------*/
static int dsFileSize(const char *path, long long *size)       // 1 = a regular file (links followed)
{
    struct stat st;

    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
    {
        return 0;
    }
    if (size != NULL)
    {
        *size = (long long)st.st_size;
    }
    return 1;
}

static int dsIsDir(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void dsJoin(char *out, size_t size, const char *dir, const char *name)
{
    size_t n = strlen(dir);

    if (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
    {
        snprintf(out, size, "%s%s", dir, name);
    }
    else
    {
        snprintf(out, size, "%s" DS_SEP "%s", dir, name);
    }
}

static void dsTrimSlashes(char *path)
{
    size_t n = strlen(path);

    while (n > 1 && (path[n - 1] == '/' || path[n - 1] == '\\') && !(n == 3 && path[1] == ':'))
    {
        path[--n] = '\0';
    }
}

static void dsTrimLine(char *s)
{
    size_t n = strlen(s);

    while (n > 0 && isspace((unsigned char)s[n - 1]))
    {
        s[--n] = '\0';
    }
}

static const char *dsBaseName(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');

    if (back != NULL && (slash == NULL || back > slash))
    {
        slash = back;
    }
    return (slash != NULL) ? slash + 1 : path;
}

static void dsListAppend(char *list, size_t size, const char *name)
{
    size_t n = strlen(list);

    snprintf(list + n, size - n, "%s%s", (n > 0) ? ", " : "", name);
}

static int dsMakeDirs(const char *path)
{
    char p[DS_PATH];
    size_t i;

    snprintf(p, sizeof(p), "%s", path);
    for (i = 1; p[i] != '\0'; i++)
    {
        if (p[i] == '/' || p[i] == '\\')
        {
            char c = p[i];

            p[i] = '\0';
            if (!(i == 2 && p[1] == ':'))
            {
                dsMkdir(p);
            }
            p[i] = c;
        }
    }
    dsMkdir(p);
    return dsIsDir(path);
}

// depth first over a folder: fn(path, isDir, size) after a folder's contents; a link is never followed
typedef void (*dsWalkFn)(const char *path, int isDir, long long size, void *data);

static void dsWalk(const char *path, dsWalkFn fn, void *data)
{
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);

    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        char pattern[DS_PATH], child[DS_PATH];
        WIN32_FIND_DATAA found;
        HANDLE h;

        dsJoin(pattern, sizeof(pattern), path, "*");
        h = FindFirstFileA(pattern, &found);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (strcmp(found.cFileName, ".") != 0 && strcmp(found.cFileName, "..") != 0)
                {
                    dsJoin(child, sizeof(child), path, found.cFileName);
                    dsWalk(child, fn, data);
                }
            } while (FindNextFileA(h, &found));
            FindClose(h);
        }
        fn(path, 1, 0, data);
    }
    else
    {
        long long size = 0;

        dsFileSize(path, &size);
        fn(path, (attributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0, size, data);
    }
#else
    struct stat st;

    if (lstat(path, &st) != 0)
    {
        return;
    }
    if (S_ISDIR(st.st_mode))
    {
        char child[DS_PATH];
        struct dirent *entry;
        DIR *dir = opendir(path);

        if (dir != NULL)
        {
            while ((entry = readdir(dir)) != NULL)
            {
                if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
                {
                    dsJoin(child, sizeof(child), path, entry->d_name);
                    dsWalk(child, fn, data);
                }
            }
            closedir(dir);
        }
        fn(path, 1, 0, data);
    }
    else
    {
        fn(path, 0, (long long)st.st_size, data);
    }
#endif
}

static void dsRemoveOne(const char *path, int isDir, long long size, void *data)
{
    int *failures = (int *)data;
    int failed;

    (void)size;
#ifdef _WIN32
    if (isDir)
    {
        failed = !RemoveDirectoryA(path);
    }
    else
    {
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        failed = !DeleteFileA(path);
    }
#else
    failed = isDir ? (rmdir(path) != 0) : (unlink(path) != 0);
#endif
    if (failed && (*failures)++ == 0)
    {
        dsLog("cannot remove %s", path);
    }
}

static void dsRemoveTree(const char *path)                     // only ever our own temp folder
{
    int failures = 0;

    dsWalk(path, dsRemoveOne, &failures);
}

typedef struct
{
    long long bytes;
    int files;
} dsTreeSize;

static void dsCountOne(const char *path, int isDir, long long size, void *data)
{
    dsTreeSize *total = (dsTreeSize *)data;

    (void)path;
    if (!isDir)
    {
        total->bytes += size;
        total->files++;
    }
}

static long long dsFreeBytes(const char *dir)
{
#ifdef _WIN32
    ULARGE_INTEGER available;

    if (GetDiskFreeSpaceExA(dir, &available, NULL, NULL))
    {
        return (long long)available.QuadPart;
    }
    return -1;
#else
    struct statvfs sv;

    if (statvfs(dir, &sv) == 0)
    {
        return (long long)sv.f_bavail * (long long)sv.f_frsize;
    }
    return -1;
#endif
}

#ifdef _WIN32
// a wide path in the engine's char encoding (the ANSI code page); its short name when a character does not fit
static int dsWideToEngine(const wchar_t *wide, char *out, size_t size)
{
    BOOL lossy = FALSE;
    int utf8 = (GetACP() == CP_UTF8);
    wchar_t shortPath[DS_PATH];
    DWORD length;

    if (WideCharToMultiByte(CP_ACP, utf8 ? 0 : WC_NO_BEST_FIT_CHARS, wide, -1, out, (int)size, NULL,
                            utf8 ? NULL : &lossy) > 0 && !lossy)
    {
        return 1;
    }
    length = GetShortPathNameW(wide, shortPath, DS_PATH);
    lossy = FALSE;
    if (length > 0 && length < DS_PATH &&
        WideCharToMultiByte(CP_ACP, utf8 ? 0 : WC_NO_BEST_FIT_CHARS, shortPath, -1, out, (int)size, NULL,
                            utf8 ? NULL : &lossy) > 0 && !lossy)
    {
        dsLog("a path has characters outside the ANSI code page: using its short name %s", out);
        return 1;
    }
    out[0] = '\0';
    return 0;
}

static int dsEngineToWide(const char *text, wchar_t *out, int count)
{
    return MultiByteToWideChar(CP_ACP, 0, text, -1, out, count) > 0;
}
#endif

/*-----------------------------------------------------------------------------
    The game folder
-----------------------------------------------------------------------------*/
// 1 when dir holds every file the engine reads; missing gets the names it lacks
static int dsValidate(const char *dir, char *missing, size_t missingSize, int verbose)
{
    char path[DS_PATH], lacking[256];
    long long bigSize = -1;
    int i;

    lacking[0] = '\0';
    for (i = 0; dsNeededFiles[i] != NULL; i++)
    {
        long long size = 0;

        dsJoin(path, sizeof(path), dir, dsNeededFiles[i]);
        if (dsFileSize(path, &size))
        {
            if (i == 0)
            {
                bigSize = size;
            }
        }
        else
        {
            dsListAppend(lacking, sizeof(lacking), dsNeededFiles[i]);
        }
    }
    for (i = 0; dsNeededDirs[i] != NULL; i++)
    {
        dsJoin(path, sizeof(path), dir, dsNeededDirs[i]);
        if (!dsIsDir(path))
        {
            dsListAppend(lacking, sizeof(lacking), dsNeededDirs[i]);
        }
    }
    if (missing != NULL)
    {
        snprintf(missing, missingSize, "%s", lacking);
    }
    if (verbose)
    {
        if (lacking[0] != '\0')
        {
            dsLog("%s: not the game folder (missing: %s)", dir, lacking);
        }
        else if (bigSize == DS_GOG_BIG_SIZE)
        {
            dsLog("%s: the game files (cataclysm.big %lld bytes: GOG 1.01)", dir, bigSize);
        }
        else
        {
            dsLog("%s: the game files, but cataclysm.big is %lld bytes, not GOG 1.01's %lld: allowed (another "
                  "release may not work)", dir, bigSize, DS_GOG_BIG_SIZE);
        }
    }
    return lacking[0] == '\0';
}

// the settings folder (utyGameSystemsPreInit's: keep the two in step)
static int dsSettingsDir(char *out, size_t size)
{
    const char *base;

    if (fileUserSettingsPath[0] != '\0')
    {
        snprintf(out, size, "%s", fileUserSettingsPath);
        return 1;
    }
#ifdef _WIN32
    base = getenv("APPDATA");
    if (base != NULL && base[0] != '\0')
    {
        snprintf(out, size, "%s\\" HWI_WIN_CONFIGDIR, base);
        return 1;
    }
#else
    base = getenv("HOME");
    if (base != NULL && base[0] != '\0')
    {
        snprintf(out, size, "%s/" CONFIGDIR, base);
        return 1;
    }
#endif
    return 0;
}

// the per-user folder that holds GameData (the unpacked installer) and the temp folder of an unpack
static int dsUserDataDir(char *out, size_t size)
{
#ifdef _WIN32
    const char *local = getenv("LOCALAPPDATA");

    if (local == NULL || local[0] == '\0')
    {
        return 0;
    }
    snprintf(out, size, "%s\\Homeworld-Infection", local);
#elif defined(__APPLE__)
    const char *home = getenv("HOME");

    if (home == NULL || home[0] == '\0')
    {
        return 0;
    }
    snprintf(out, size, "%s/Library/Application Support/Homeworld-Infection", home);
#else
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");

    if (xdg != NULL && xdg[0] == '/')
    {
        snprintf(out, size, "%s/homeworld-infection", xdg);
    }
    else if (home != NULL && home[0] != '\0')
    {
        snprintf(out, size, "%s/.local/share/homeworld-infection", home);
    }
    else
    {
        return 0;
    }
#endif
    return 1;
}

static int dsGameDataDir(char *out, size_t size)
{
    char base[DS_PATH];

    if (!dsUserDataDir(base, sizeof(base)))
    {
        return 0;
    }
    dsJoin(out, size, base, "GameData");
    return 1;
}

static int dsSavedPathFile(char *out, size_t size)
{
    char settings[DS_PATH];

    if (!dsSettingsDir(settings, sizeof(settings)))
    {
        return 0;
    }
    dsJoin(out, size, settings, "gamedata-path.txt");
    return 1;
}

static int dsSavedPathRead(const char *file, char *out, size_t size)
{
    FILE *f = fopen(file, "r");

    if (f == NULL)
    {
        return 0;
    }
    if (fgets(out, (int)size, f) == NULL)
    {
        out[0] = '\0';
    }
    fclose(f);
    dsTrimLine(out);
    dsTrimSlashes(out);
    return out[0] != '\0';
}

static void dsSavedPathWrite(const char *dir)
{
    char settings[DS_PATH], file[DS_PATH];
    FILE *f;

    if (!dsSettingsDir(settings, sizeof(settings)))
    {
        dsLog("no settings folder (HOME / APPDATA is not set): the choice is not saved");
        return;
    }
    dsMakeDirs(settings);
    dsJoin(file, sizeof(file), settings, "gamedata-path.txt");
    f = fopen(file, "w");
    if (f == NULL)
    {
        dsLog("cannot write %s: %s", file, strerror(errno));
        return;
    }
    fprintf(f, "%s\n", dir);
    fclose(f);
    dsLog("saved the path in %s", file);
}

/*-----------------------------------------------------------------------------
    Child processes: innoextract, the pickers, the file manager
-----------------------------------------------------------------------------*/
typedef struct
{
#ifdef _WIN32
    HANDLE process;
    HANDLE out;
#else
    pid_t pid;
    int out;
#endif
} dsProc;

#ifdef _WIN32
static void dsWideAppend(wchar_t *cmd, size_t capacity, size_t *length, wchar_t c, size_t times)
{
    while (times-- > 0 && *length + 1 < capacity)
    {
        cmd[(*length)++] = c;
    }
    cmd[*length] = L'\0';
}

// one argument of a command line, quoted by the C runtime's rules
static void dsWideQuote(wchar_t *cmd, size_t capacity, size_t *length, const wchar_t *arg)
{
    const wchar_t *c = arg;

    if (*length > 0)
    {
        dsWideAppend(cmd, capacity, length, L' ', 1);
    }
    dsWideAppend(cmd, capacity, length, L'"', 1);
    for (;;)
    {
        size_t slashes = 0;

        while (*c == L'\\')
        {
            slashes++;
            c++;
        }
        if (*c == L'\0')
        {
            dsWideAppend(cmd, capacity, length, L'\\', slashes * 2);
            break;
        }
        if (*c == L'"')
        {
            dsWideAppend(cmd, capacity, length, L'\\', slashes * 2 + 1);
        }
        else
        {
            dsWideAppend(cmd, capacity, length, L'\\', slashes);
        }
        dsWideAppend(cmd, capacity, length, *c, 1);
        c++;
    }
    dsWideAppend(cmd, capacity, length, L'"', 1);
}
#endif

// starts argv (in the engine's char encoding) with its output and errors in one pipe; no console window on Windows
static int dsProcStart(dsProc *proc, const char *const argv[])
{
#ifdef _WIN32
    SECURITY_ATTRIBUTES inherit = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    STARTUPINFOW startup;
    PROCESS_INFORMATION info;
    HANDLE readEnd = NULL, writeEnd = NULL, nul;
    size_t capacity = 32768, length = 0;
    wchar_t *cmd = (wchar_t *)calloc(capacity, sizeof(wchar_t));
    wchar_t arg[DS_PATH];
    BOOL started;
    int i;

    proc->process = NULL;
    proc->out = NULL;
    if (cmd == NULL)
    {
        return 0;
    }
    for (i = 0; argv[i] != NULL; i++)
    {
        if (!dsEngineToWide(argv[i], arg, DS_PATH))
        {
            free(cmd);
            return 0;
        }
        dsWideQuote(cmd, capacity, &length, arg);
    }
    if (!CreatePipe(&readEnd, &writeEnd, &inherit, 0))
    {
        free(cmd);
        return 0;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, NULL);
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nul;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;
    started = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &info);
    CloseHandle(writeEnd);
    if (nul != INVALID_HANDLE_VALUE)
    {
        CloseHandle(nul);
    }
    free(cmd);
    if (!started)
    {
        dsLog("cannot start %s (error %lu)", argv[0], (unsigned long)GetLastError());
        CloseHandle(readEnd);
        return 0;
    }
    CloseHandle(info.hThread);
    proc->process = info.hProcess;
    proc->out = readEnd;
    return 1;
#else
    posix_spawn_file_actions_t actions;
    int fds[2], rc;

    proc->pid = -1;
    proc->out = -1;
    if (pipe(fds) != 0)
    {
        return 0;
    }
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 2);
    posix_spawn_file_actions_addclose(&actions, fds[0]);
    posix_spawn_file_actions_addclose(&actions, fds[1]);
    rc = (strchr(argv[0], '/') != NULL)
        ? posix_spawn(&proc->pid, argv[0], &actions, NULL, (char *const *)argv, environ)
        : posix_spawnp(&proc->pid, argv[0], &actions, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fds[1]);
    if (rc != 0)
    {
        dsLog("cannot start %s: %s", argv[0], strerror(rc));
        close(fds[0]);
        proc->pid = -1;
        return 0;
    }
    proc->out = fds[0];
    return 1;
#endif
}

// >0: bytes read; 0: nothing within waitMs; -1: the output has ended
static int dsProcRead(dsProc *proc, char *buf, int size, int waitMs)
{
#ifdef _WIN32
    DWORD available = 0, got = 0;

    if (!PeekNamedPipe(proc->out, NULL, 0, NULL, &available, NULL))
    {
        return -1;
    }
    if (available == 0)
    {
        WaitForSingleObject(proc->process, (DWORD)waitMs);
        if (!PeekNamedPipe(proc->out, NULL, 0, NULL, &available, NULL))
        {
            return -1;
        }
        if (available == 0)
        {
            return 0;
        }
    }
    if (!ReadFile(proc->out, buf, (available < (DWORD)size) ? available : (DWORD)size, &got, NULL))
    {
        return -1;
    }
    return (int)got;
#else
    struct pollfd fd;
    ssize_t got;

    fd.fd = proc->out;
    fd.events = POLLIN;
    fd.revents = 0;
    if (poll(&fd, 1, waitMs) <= 0)
    {
        return 0;
    }
    got = read(proc->out, buf, (size_t)size);
    if (got < 0 && (errno == EINTR || errno == EAGAIN))
    {
        return 0;
    }
    return (got > 0) ? (int)got : -1;
#endif
}

// the exit code (256 + the signal when killed)
static int dsProcWait(dsProc *proc)
{
#ifdef _WIN32
    DWORD code = 1;

    CloseHandle(proc->out);
    WaitForSingleObject(proc->process, INFINITE);
    GetExitCodeProcess(proc->process, &code);
    CloseHandle(proc->process);
    return (int)code;
#else
    int status = 0;

    close(proc->out);
    while (waitpid(proc->pid, &status, 0) < 0 && errno == EINTR)
    {
    }
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    return 256 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
#endif
}

static void dsProcKill(dsProc *proc)
{
#ifdef _WIN32
    TerminateProcess(proc->process, 1);
#else
    kill(proc->pid, SIGTERM);
#endif
}

// runs argv to its end: its output in out (and on POSIX its errors in err, apart); the exit code, -1 = not started
static int dsRunCapture(const char *const argv[], char *out, size_t outSize, char *err, size_t errSize)
{
    size_t outLength = 0, errLength = 0;

    out[0] = '\0';
    err[0] = '\0';
#ifdef _WIN32
    {
        dsProc proc;
        char buf[1024];
        int got;

        if (!dsProcStart(&proc, argv))
        {
            return -1;
        }
        while ((got = dsProcRead(&proc, buf, sizeof(buf), 100)) >= 0)
        {
            if (got > 0 && outLength + 1 < outSize)
            {
                size_t take = ((size_t)got < outSize - 1 - outLength) ? (size_t)got : outSize - 1 - outLength;

                memcpy(out + outLength, buf, take);
                outLength += take;
                out[outLength] = '\0';
            }
        }
        (void)errLength;
        (void)errSize;
        return dsProcWait(&proc);
    }
#else
    {
        posix_spawn_file_actions_t actions;
        int po[2], pe[2], rc, status = 0, fo, fe;
        pid_t pid;

        if (pipe(po) != 0)
        {
            return -1;
        }
        if (pipe(pe) != 0)
        {
            close(po[0]);
            close(po[1]);
            return -1;
        }
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_adddup2(&actions, po[1], 1);
        posix_spawn_file_actions_adddup2(&actions, pe[1], 2);
        posix_spawn_file_actions_addclose(&actions, po[0]);
        posix_spawn_file_actions_addclose(&actions, po[1]);
        posix_spawn_file_actions_addclose(&actions, pe[0]);
        posix_spawn_file_actions_addclose(&actions, pe[1]);
        rc = (strchr(argv[0], '/') != NULL)
            ? posix_spawn(&pid, argv[0], &actions, NULL, (char *const *)argv, environ)
            : posix_spawnp(&pid, argv[0], &actions, NULL, (char *const *)argv, environ);
        posix_spawn_file_actions_destroy(&actions);
        close(po[1]);
        close(pe[1]);
        if (rc != 0)
        {
            close(po[0]);
            close(pe[0]);
            return -1;
        }
        fo = po[0];
        fe = pe[0];
        while (fo >= 0 || fe >= 0)
        {
            struct pollfd fds[2];
            int count = 0, i;

            if (fo >= 0)
            {
                fds[count].fd = fo;
                fds[count].events = POLLIN;
                fds[count].revents = 0;
                count++;
            }
            if (fe >= 0)
            {
                fds[count].fd = fe;
                fds[count].events = POLLIN;
                fds[count].revents = 0;
                count++;
            }
            if (poll(fds, (nfds_t)count, 200) < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                break;
            }
            for (i = 0; i < count; i++)
            {
                char buf[1024];
                ssize_t got;
                int isOut = (fds[i].fd == fo);
                char *dest = isOut ? out : err;
                size_t *length = isOut ? &outLength : &errLength;
                size_t destSize = isOut ? outSize : errSize;

                if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
                {
                    continue;
                }
                got = read(fds[i].fd, buf, sizeof(buf));
                if (got <= 0)
                {
                    close(fds[i].fd);
                    if (isOut)
                    {
                        fo = -1;
                    }
                    else
                    {
                        fe = -1;
                    }
                    continue;
                }
                if (*length + 1 < destSize)
                {
                    size_t take = ((size_t)got < destSize - 1 - *length) ? (size_t)got : destSize - 1 - *length;

                    memcpy(dest + *length, buf, take);
                    *length += take;
                    dest[*length] = '\0';
                }
            }
        }
        if (fo >= 0)
        {
            close(fo);
        }
        if (fe >= 0)
        {
            close(fe);
        }
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
        {
        }
        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        return 256 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    }
#endif
}

// the first line of text, for a log line
static void dsFirstLine(const char *text, char *out, size_t size)
{
    size_t n = strcspn(text, "\r\n");

    if (n >= size)
    {
        n = size - 1;
    }
    memcpy(out, text, n);
    out[n] = '\0';
}

/*-----------------------------------------------------------------------------
    Dialogs
-----------------------------------------------------------------------------*/
// why this run cannot show a dialog, or NULL
static const char *dsNoDialogReason(void)
{
    static char reason[160];
    const char *value = getenv("HWI_DATASETUP_NONINTERACTIVE");
    const char *driver;

    if (dsScriptOn)
    {
        return NULL;
    }
    if (value != NULL && value[0] != '\0' && strcmp(value, "0") != 0)
    {
        return "HWI_DATASETUP_NONINTERACTIVE is set";
    }
    driver = SDL_GetHint(SDL_HINT_VIDEODRIVER);                 // the hint or SDL_VIDEODRIVER
    if (driver != NULL && driver[0] != '\0')
    {
        char first[64];
        size_t n = strcspn(driver, ",");

        snprintf(first, sizeof(first), "%.*s", (int)((n < sizeof(first)) ? n : sizeof(first) - 1), driver);
        if (SDL_strcasecmp(first, "offscreen") == 0 || SDL_strcasecmp(first, "dummy") == 0)
        {
            snprintf(reason, sizeof(reason), "the SDL video driver is %s", first);
            return reason;
        }
    }
#if !defined(_WIN32) && !defined(__APPLE__)
    {
        const char *x11 = getenv("DISPLAY");
        const char *wayland = getenv("WAYLAND_DISPLAY");

        if ((x11 == NULL || x11[0] == '\0') && (wayland == NULL || wayland[0] == '\0'))
        {
            return "no display: DISPLAY and WAYLAND_DISPLAY are not set";
        }
    }
#endif
    return NULL;
}

static void dsLogText(const char *prefix, const char *text)
{
    char line[512];
    const char *p = text;

    while (*p != '\0')
    {
        size_t n = strcspn(p, "\n");

        snprintf(line, sizeof(line), "%.*s", (int)((n < sizeof(line)) ? n : sizeof(line) - 1), p);
        dsLog("%s%s", prefix, line);
        p += n;
        if (*p == '\n')
        {
            p++;
        }
    }
}

#define DS_BOX_CLOSED   -1                          // closed, Escape: the same as Quit
#define DS_BOX_FAILED   -2                          // no dialog could be shown

// a dialog: the index of the button chosen, DS_BOX_CLOSED or DS_BOX_FAILED. keys: the scripted answer of each button
static int dsBox(Uint32 kind, const char *text, const char *const buttons[], const char *const keys[], int count,
                 int enterButton, int escapeButton)
{
    SDL_MessageBoxButtonData data[4];
    SDL_MessageBoxData box;
    char key[64], list[256];
    int i, hit = -1;

    list[0] = '\0';
    for (i = 0; i < count; i++)
    {
        dsListAppend(list, sizeof(list), buttons[i]);
    }
    if (dsScriptOn)
    {
        dsLog("[test] a dialog \"%s\" (buttons: %s) would show:", DS_TITLE, list);
        dsLogText("[test]   | ", text);
        if (count <= 1)
        {
            return 0;
        }
        if (!dsScriptNext(key, sizeof(key)))
        {
            dsLog("[test] no scripted answer left: %s", buttons[escapeButton]);
            return escapeButton;
        }
        for (i = 0; i < count; i++)
        {
            if (strcmp(key, keys[i]) == 0)
            {
                dsLog("[test] answer \"%s%s%s\": %s", key, dsScriptArg[0] ? ":" : "", dsScriptArg, buttons[i]);
                return i;
            }
        }
        dsLog("[test] the answer \"%s\" fits no button: %s", key, buttons[escapeButton]);
        return escapeButton;
    }
    if (dsNoDialogReason() != NULL)
    {
        dsLog("cannot show a dialog (%s)", dsNoDialogReason());
        return DS_BOX_FAILED;
    }
    for (i = 0; i < count && i < 4; i++)
    {
        data[i].flags = 0;
        if (i == enterButton)
        {
            data[i].flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        }
        if (i == escapeButton)
        {
            data[i].flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
        }
        data[i].buttonid = i;
        data[i].text = buttons[i];
    }
    memset(&box, 0, sizeof(box));
    box.flags = kind | SDL_MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT;
    box.title = DS_TITLE;
    box.message = text;
    box.numbuttons = (count < 4) ? count : 4;
    box.buttons = data;
    dsLog("showing a dialog (buttons: %s):", list);
    dsLogText("  | ", text);
    if (SDL_ShowMessageBox(&box, &hit) < 0)
    {
        dsLog("the dialog could not be shown: %s", SDL_GetError());
        return DS_BOX_FAILED;
    }
    if (hit < 0 || hit >= count)
    {
        dsLog("the dialog was closed");
        return DS_BOX_CLOSED;
    }
    dsLog("chosen: %s", buttons[hit]);
    return hit;
}

static int dsMessage(Uint32 kind, const char *text)
{
    static const char *const ok[] = { "OK" };
    static const char *const okKeys[] = { "ok" };

    return dsBox(kind, text, ok, okKeys, 1, 0, 0);
}

static void dsOpenFolder(const char *dir)
{
    if (dsScriptOn)
    {
        dsLog("[test] would open %s in the file manager", dir);
        return;
    }
#ifdef _WIN32
    {
        wchar_t wide[DS_PATH];

        if (dsEngineToWide(dir, wide, DS_PATH))
        {
            ShellExecuteW(NULL, L"open", wide, NULL, NULL, SW_SHOWNORMAL);
        }
    }
#else
    {
        char out[256], err[256];
#ifdef __APPLE__
        const char *argv[] = { "open", dir, NULL };
#else
        const char *argv[] = { "sh", "-c", "xdg-open \"$1\" >/dev/null 2>&1 &", "sh", dir, NULL };
#endif
        dsRunCapture(argv, out, sizeof(out), err, sizeof(err));
    }
#endif
    dsLog("opened %s in the file manager", dir);
}

/*-----------------------------------------------------------------------------
    Pickers: 1 = chosen (the path in out), 0 = cancelled, -1 = no picker on this computer
-----------------------------------------------------------------------------*/
#ifdef _WIN32
// the shell's IFileOpenDialog (no CLSID or IID from libuuid: these are the SDK's values)
static const CLSID dsCLSID_FileOpenDialog = { 0xdc1c5a9c, 0xe88a, 0x4dde, { 0xa5, 0xa1, 0x60, 0xf8, 0x2a, 0x20, 0xae, 0xf7 } };
static const IID dsIID_IFileOpenDialog = { 0xd57c7288, 0xd4ad, 0x4768, { 0xbe, 0x02, 0x9d, 0x96, 0x95, 0x32, 0xd9, 0x60 } };
static const IID dsIID_IShellItem = { 0x43826d1e, 0xe718, 0x42ee, { 0xbc, 0x55, 0xa1, 0xe2, 0x61, 0xc3, 0x7b, 0xfe } };
static const GUID dsFOLDERID_Downloads = { 0x374de290, 0x123f, 0x4565, { 0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b } };
static const GUID dsFOLDERID_Desktop = { 0xb4bfcc3a, 0xdb2c, 0x424c, { 0xb0, 0x29, 0x7f, 0xe9, 0x9a, 0x87, 0xc6, 0x41 } };

static int dsKnownFolder(const GUID *id, char *out, size_t size)
{
    PWSTR wide = NULL;
    int ok = 0;

    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, NULL, &wide)))
    {
        ok = dsWideToEngine(wide, out, size);
    }
    if (wide != NULL)
    {
        CoTaskMemFree(wide);
    }
    return ok;
}

static int dsPickWindows(int folder, char *out, size_t size)
{
    IFileOpenDialog *dialog = NULL;
    IShellItem *item = NULL;
    PWSTR wide = NULL;
    DWORD options = 0;
    HRESULT hr, init;
    int result = -1;

    init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    hr = CoCreateInstance(&dsCLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &dsIID_IFileOpenDialog,
                          (void **)&dialog);
    if (FAILED(hr))
    {
        dsLog("no file dialog (0x%08lx)", (unsigned long)hr);
        goto done;
    }
    IFileOpenDialog_GetOptions(dialog, &options);
    options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | (folder ? FOS_PICKFOLDERS : FOS_FILEMUSTEXIST);
    IFileOpenDialog_SetOptions(dialog, options);
    if (folder)
    {
        IFileOpenDialog_SetTitle(dialog, L"Choose the folder where GOG installed Homeworld: Emergence (it holds cataclysm.big)");
    }
    else
    {
        COMDLG_FILTERSPEC types[2];
        PWSTR downloads = NULL;

        types[0].pszName = L"GOG offline installer (setup_homeworld_emergence*.exe)";
        types[0].pszSpec = L"setup_homeworld_emergence*.exe";
        types[1].pszName = L"Programs (*.exe)";
        types[1].pszSpec = L"*.exe";
        IFileOpenDialog_SetFileTypes(dialog, 2, types);
        IFileOpenDialog_SetTitle(dialog, L"Choose the GOG offline installer of Homeworld: Emergence");
        if (SUCCEEDED(SHGetKnownFolderPath(&dsFOLDERID_Downloads, 0, NULL, &downloads)))
        {
            IShellItem *start = NULL;

            if (SUCCEEDED(SHCreateItemFromParsingName(downloads, NULL, &dsIID_IShellItem, (void **)&start)))
            {
                IFileOpenDialog_SetFolder(dialog, start);
                IShellItem_Release(start);
            }
        }
        if (downloads != NULL)
        {
            CoTaskMemFree(downloads);
        }
    }
    hr = IFileOpenDialog_Show(dialog, NULL);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
    {
        result = 0;
        goto done;
    }
    if (FAILED(hr) || FAILED(IFileOpenDialog_GetResult(dialog, &item)) ||
        FAILED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &wide)))
    {
        dsLog("the file dialog failed (0x%08lx)", (unsigned long)hr);
        goto done;
    }
    if (dsWideToEngine(wide, out, size))
    {
        result = 1;
    }
    else
    {
        dsMessage(SDL_MESSAGEBOX_ERROR, "That path has characters this game cannot use.\n\n"
                                        "Move the folder or the installer to a folder with a plain name\n"
                                        "(letters, digits and spaces), then choose it again.");
        result = 0;
    }
done:
    if (wide != NULL)
    {
        CoTaskMemFree(wide);
    }
    if (item != NULL)
    {
        IShellItem_Release(item);
    }
    if (dialog != NULL)
    {
        IFileOpenDialog_Release(dialog);
    }
    if (SUCCEEDED(init))
    {
        CoUninitialize();
    }
    return result;
}
#else
// a picker program: its stdout is the path; exit 1 = cancelled
static int dsRunPicker(const char *const argv[], char *out, size_t size)
{
    char err[1024], first[256];
    int rc = dsRunCapture(argv, out, size, err, sizeof(err));

    dsTrimLine(out);
    dsFirstLine(out, out, size);
    if (rc == -1 || rc == 127)
    {
        dsLog("no %s on this computer", argv[0]);
        return -1;
    }
    if (rc == 0 && out[0] != '\0')
    {
        dsTrimSlashes(out);
        dsLog("%s: chosen %s", argv[0], out);
        return 1;
    }
#ifdef __APPLE__
    if (rc == 1 && strstr(err, "-128") != NULL)                // osascript: "User canceled. (-128)"
#else
    if (rc == 1 && strstr(err, "isplay") == NULL)              // not "cannot open display"
#endif
    {
        dsLog("%s: cancelled", argv[0]);
        return 0;
    }
    dsFirstLine(err, first, sizeof(first));
    dsLog("%s failed (exit %d): %s", argv[0], rc, first);
    return -1;
}

#ifndef __APPLE__
// XDG_<name>_DIR of user-dirs.dirs (Downloads, Desktop in the player's language)
static int dsXdgUserDir(const char *name, char *out, size_t size)
{
    const char *config = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    char file[DS_PATH], line[DS_PATH], key[64];
    FILE *f;

    if (config != NULL && config[0] == '/')
    {
        dsJoin(file, sizeof(file), config, "user-dirs.dirs");
    }
    else if (home != NULL && home[0] != '\0')
    {
        snprintf(file, sizeof(file), "%s/.config/user-dirs.dirs", home);
    }
    else
    {
        return 0;
    }
    f = fopen(file, "r");
    if (f == NULL)
    {
        return 0;
    }
    snprintf(key, sizeof(key), "XDG_%s_DIR=", name);
    while (fgets(line, sizeof(line), f) != NULL)
    {
        char *p = line, *quote;

        while (isspace((unsigned char)*p))
        {
            p++;
        }
        if (strncmp(p, key, strlen(key)) != 0)
        {
            continue;
        }
        p += strlen(key);
        dsTrimLine(p);
        if (*p == '"')
        {
            p++;
        }
        quote = strrchr(p, '"');
        if (quote != NULL)
        {
            *quote = '\0';
        }
        if (strncmp(p, "$HOME", 5) == 0 && home != NULL)
        {
            snprintf(out, size, "%s%s", home, p + 5);
        }
        else if (p[0] == '/')
        {
            snprintf(out, size, "%s", p);
        }
        else
        {
            continue;
        }
        fclose(f);
        dsTrimSlashes(out);
        return 1;
    }
    fclose(f);
    return 0;
}
#endif
#endif

#ifndef _WIN32
// the Downloads folder (Windows: dsKnownFolder)
static int dsDownloadsDir(char *out, size_t size)
{
    const char *home = getenv("HOME");

#ifndef __APPLE__
    if (dsXdgUserDir("DOWNLOAD", out, size))
    {
        return 1;
    }
#endif
    if (home == NULL || home[0] == '\0')
    {
        return 0;
    }
    snprintf(out, size, "%s/Downloads", home);
    return 1;
}
#endif

static int dsPickFolder(char *out, size_t size)
{
#ifdef _WIN32
    return dsPickWindows(1, out, size);
#elif defined(__APPLE__)
    const char *argv[] = { "osascript", "-e",
        "POSIX path of (choose folder with prompt \"Choose the folder where GOG installed Homeworld: Emergence "
        "(it holds cataclysm.big)\")", NULL };

    return dsRunPicker(argv, out, size);
#else
    const char *home = getenv("HOME");
    char start[DS_PATH + 16], startDir[DS_PATH];
    int rc;

    snprintf(startDir, sizeof(startDir), "%s", (home != NULL) ? home : "/");
    snprintf(start, sizeof(start), "--filename=%s/", startDir);
    {
        const char *zenity[] = { "zenity", "--file-selection", "--directory",
                                 "--title=Choose the folder where GOG installed Homeworld: Emergence (it holds cataclysm.big)",
                                 start, NULL };

        rc = dsRunPicker(zenity, out, size);
        if (rc >= 0)
        {
            return rc;
        }
    }
    {
        const char *kdialog[] = { "kdialog", "--title",
                                  "Choose the folder where GOG installed Homeworld: Emergence (it holds cataclysm.big)",
                                  "--getexistingdirectory", startDir, NULL };

        return dsRunPicker(kdialog, out, size);
    }
#endif
}

static int dsPickInstaller(char *out, size_t size)
{
#ifdef _WIN32
    return dsPickWindows(0, out, size);
#elif defined(__APPLE__)
    const char *argv[] = { "osascript", "-e",
        "POSIX path of (choose file with prompt \"Choose the GOG offline installer of Homeworld: Emergence "
        "(setup_homeworld_emergence....exe)\" default location (path to downloads folder))", NULL };

    return dsRunPicker(argv, out, size);
#else
    char start[DS_PATH + 16], startDir[DS_PATH];
    int rc;

    if (!dsDownloadsDir(startDir, sizeof(startDir)) || !dsIsDir(startDir))
    {
        const char *home = getenv("HOME");

        snprintf(startDir, sizeof(startDir), "%s", (home != NULL) ? home : "/");
    }
    snprintf(start, sizeof(start), "--filename=%s/", startDir);
    {
        const char *zenity[] = { "zenity", "--file-selection",
                                 "--title=Choose the GOG offline installer of Homeworld: Emergence",
                                 "--file-filter=GOG offline installer | setup_homeworld_emergence*.exe",
                                 "--file-filter=Programs | *.exe", "--file-filter=All files | *", start, NULL };

        rc = dsRunPicker(zenity, out, size);
        if (rc >= 0)
        {
            return rc;
        }
    }
    {
        const char *kdialog[] = { "kdialog", "--title", "Choose the GOG offline installer of Homeworld: Emergence",
                                  "--getopenfilename", startDir,
                                  "setup_homeworld_emergence*.exe *.exe|GOG offline installer", NULL };

        return dsRunPicker(kdialog, out, size);
    }
#endif
}

/*-----------------------------------------------------------------------------
    Auto-detect: an installed GOG copy
-----------------------------------------------------------------------------*/
#define DS_MAX_FOUND 8

typedef struct
{
    char path[DS_MAX_FOUND][DS_PATH];
    char real[DS_MAX_FOUND][DS_PATH];
    int count;
    int checked;
} dsFound;

static void dsConsider(dsFound *found, const char *dir)
{
    char real[DS_PATH];
    int i;

    found->checked++;
    if (!dsIsDir(dir) || !dsValidate(dir, NULL, 0, 1) || found->count >= DS_MAX_FOUND)
    {
        return;
    }
#ifdef _WIN32
    if (_fullpath(real, dir, sizeof(real)) == NULL)
#else
    if (realpath(dir, real) == NULL)
#endif
    {
        snprintf(real, sizeof(real), "%s", dir);
    }
    for (i = 0; i < found->count; i++)
    {
#ifdef _WIN32
        if (SDL_strcasecmp(found->real[i], real) == 0)
#else
        if (strcmp(found->real[i], real) == 0)
#endif
        {
            return;
        }
    }
    snprintf(found->path[found->count], DS_PATH, "%s", dir);
    snprintf(found->real[found->count], DS_PATH, "%s", real);
    found->count++;
}

#ifdef _WIN32
static void dsConsiderRegistry(dsFound *found, HKEY root, const wchar_t *key, REGSAM view, const char *name)
{
    wchar_t value[DS_PATH];
    DWORD type = 0, bytes = sizeof(value) - sizeof(wchar_t);
    char path[DS_PATH];
    HKEY handle;
    LONG rc;

    if (RegOpenKeyExW(root, key, 0, KEY_QUERY_VALUE | view, &handle) != ERROR_SUCCESS)
    {
        return;
    }
    rc = RegQueryValueExW(handle, L"path", NULL, &type, (BYTE *)value, &bytes);
    RegCloseKey(handle);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
    {
        return;
    }
    value[bytes / sizeof(wchar_t)] = L'\0';
    if (dsWideToEngine(value, path, sizeof(path)))
    {
        dsTrimSlashes(path);
        dsLog("registry %s: %s", name, path);
        dsConsider(found, path);
    }
}
#endif

// the installed copies, in order; out = the first. The count found
static int dsAutoDetect(char *out, size_t size)
{
    static dsFound found;                           // static: 16 KB off the stack
    char path[DS_PATH];
    int i;

    memset(&found, 0, sizeof(found));
    if (dsGameDataDir(path, sizeof(path)))          // an earlier unpack
    {
        dsConsider(&found, path);
    }
#ifdef _WIN32
    {
        static const char *const folders[] = { "%s\\GOG Games\\Homeworld Emergence", "D:\\GOG Games\\Homeworld Emergence",
                                               "%s\\GOG Galaxy\\Games\\Homeworld Emergence", NULL };
        const char *drive = getenv("SystemDrive");
        const char *programs[2];
        int p;

        // the key GOG's offline installer and GOG Galaxy both write; WOW6432Node = the 32-bit view
        dsConsiderRegistry(&found, HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\GOG.com\\Games\\" DS_GOG_GAME_ID_W,
                           KEY_WOW64_64KEY, "HKLM\\SOFTWARE\\WOW6432Node\\GOG.com\\Games\\" DS_GOG_GAME_ID);
        dsConsiderRegistry(&found, HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games\\" DS_GOG_GAME_ID_W,
                           KEY_WOW64_32KEY, "HKLM\\SOFTWARE\\GOG.com\\Games\\" DS_GOG_GAME_ID " (32-bit view)");
        dsConsiderRegistry(&found, HKEY_LOCAL_MACHINE, L"SOFTWARE\\GOG.com\\Games\\" DS_GOG_GAME_ID_W,
                           KEY_WOW64_64KEY, "HKLM\\SOFTWARE\\GOG.com\\Games\\" DS_GOG_GAME_ID " (64-bit view)");
        snprintf(path, sizeof(path), folders[0], (drive != NULL && drive[0] != '\0') ? drive : "C:");
        dsConsider(&found, path);
        dsConsider(&found, folders[1]);
        programs[0] = getenv("ProgramFiles(x86)");
        programs[1] = getenv("ProgramFiles");
        for (p = 0; p < 2; p++)
        {
            if (programs[p] != NULL && programs[p][0] != '\0')
            {
                snprintf(path, sizeof(path), folders[2], programs[p]);
                dsConsider(&found, path);
            }
        }
    }
#else
    {
        // GOG has no Linux or macOS build of this game: the Windows installer under Wine, Heroic, Lutris, Bottles,
        // Proton or CrossOver
        static const char *const patterns[] = {
            "GOG Games/Homeworld Emergence",
            "Games/*/Homeworld Emergence",                                  // Heroic: ~/Games/Heroic/<title>
            "Games/*/drive_c/GOG Games/Homeworld Emergence",                // Lutris prefixes
            "Games/*/drive_c/Program Files (x86)/GOG Galaxy/Games/Homeworld Emergence",
            ".wine/drive_c/GOG Games/Homeworld Emergence",
            ".wine/drive_c/Program Files (x86)/GOG Galaxy/Games/Homeworld Emergence",
            ".local/share/bottles/bottles/*/drive_c/GOG Games/Homeworld Emergence",
            ".var/app/com.usebottles.bottles/data/bottles/bottles/*/drive_c/GOG Games/Homeworld Emergence",
            ".local/share/Steam/steamapps/compatdata/*/pfx/drive_c/GOG Games/Homeworld Emergence",
#ifdef __APPLE__
            "Library/Application Support/CrossOver/Bottles/*/drive_c/GOG Games/Homeworld Emergence",
#endif
            NULL };
        const char *home = getenv("HOME");
        int p;

        for (p = 0; home != NULL && home[0] != '\0' && patterns[p] != NULL; p++)
        {
            glob_t matches;
            size_t m;

            dsJoin(path, sizeof(path), home, patterns[p]);
            if (strchr(patterns[p], '*') == NULL)
            {
                dsConsider(&found, path);
                continue;
            }
            if (glob(path, GLOB_NOSORT | GLOB_ONLYDIR, NULL, &matches) == 0)
            {
                for (m = 0; m < matches.gl_pathc; m++)
                {
                    dsConsider(&found, matches.gl_pathv[m]);
                }
            }
            globfree(&matches);
        }
    }
#endif
    dsLog("auto-detect: %d places checked, %d with the game files", found.checked, found.count);
    for (i = 1; i < found.count; i++)
    {
        dsLog("auto-detect: also found %s (not used: the first one wins)", found.path[i]);
    }
    if (found.count > 0)
    {
        snprintf(out, size, "%s", found.path[0]);
        dsTrimSlashes(out);
    }
    return found.count;
}

/*-----------------------------------------------------------------------------
    The offline installer
-----------------------------------------------------------------------------*/
typedef struct
{
    char best[DS_PATH];
    long long bestTime;
    char stem[DS_PATH];                             // to list the -N.bin parts of the one chosen
} dsInstallerScan;

static int dsIsInstallerName(const char *name)
{
    size_t length = strlen(name);

    return length > 4 && SDL_strncasecmp(name, DS_INSTALLER_PREFIX, strlen(DS_INSTALLER_PREFIX)) == 0 &&
           SDL_strcasecmp(name + length - 4, ".exe") == 0;
}

static void dsScanInstallers(const char *dir, dsInstallerScan *scan)
{
#ifdef _WIN32
    char pattern[DS_PATH], path[DS_PATH];
    WIN32_FIND_DATAA found;
    HANDLE h;

    dsJoin(pattern, sizeof(pattern), dir, DS_INSTALLER_PREFIX "*.exe");
    h = FindFirstFileA(pattern, &found);
    if (h == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        long long size = ((long long)found.nFileSizeHigh << 32) | found.nFileSizeLow;
        long long when = ((long long)found.ftLastWriteTime.dwHighDateTime << 32) | found.ftLastWriteTime.dwLowDateTime;

        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !dsIsInstallerName(found.cFileName))
        {
            continue;
        }
        dsJoin(path, sizeof(path), dir, found.cFileName);
#else
    char path[DS_PATH];
    struct dirent *entry;
    DIR *d = opendir(dir);

    if (d == NULL)
    {
        return;
    }
    while ((entry = readdir(d)) != NULL)
    {
        struct stat st;
        long long size, when;

        if (!dsIsInstallerName(entry->d_name))
        {
            continue;
        }
        dsJoin(path, sizeof(path), dir, entry->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
        {
            continue;
        }
        size = (long long)st.st_size;
        when = (long long)st.st_mtime;
#endif
        if (size < 1024 * 1024)
        {
            dsLog("%s: %lld bytes, too small for the offline installer: skipped", path, size);
            continue;
        }
        dsLog("offline installer candidate: %s (%lld bytes)", path, size);
        if (when > scan->bestTime)
        {
            scan->bestTime = when;
            snprintf(scan->best, sizeof(scan->best), "%s", path);
        }
#ifdef _WIN32
    } while (FindNextFileA(h, &found));
    FindClose(h);
#else
    }
    closedir(d);
#endif
}

// the installer's -N.bin parts (innoextract reads them from its folder by itself): logged
static void dsLogParts(const char *installer)
{
    char dir[DS_PATH], stem[DS_PATH], path[DS_PATH];
    const char *name = dsBaseName(installer);
    size_t stemLength;
    int parts = 0;

    snprintf(dir, sizeof(dir), "%.*s", (int)(name - installer), installer);
    if (dir[0] == '\0')
    {
        snprintf(dir, sizeof(dir), ".");
    }
    snprintf(stem, sizeof(stem), "%.*s-", (int)(strlen(name) - 4), name);
    stemLength = strlen(stem);
#ifdef _WIN32
    {
        char pattern[DS_PATH];
        WIN32_FIND_DATAA found;
        HANDLE h;

        dsJoin(pattern, sizeof(pattern), dir, "*.bin");
        h = FindFirstFileA(pattern, &found);
        if (h != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (SDL_strncasecmp(found.cFileName, stem, stemLength) == 0)
                {
                    dsJoin(path, sizeof(path), dir, found.cFileName);
                    dsLog("installer part: %s", path);
                    parts++;
                }
            } while (FindNextFileA(h, &found));
            FindClose(h);
        }
    }
#else
    {
        struct dirent *entry;
        DIR *d = opendir(dir);

        while (d != NULL && (entry = readdir(d)) != NULL)
        {
            size_t length = strlen(entry->d_name);

            if (length > stemLength + 4 && SDL_strncasecmp(entry->d_name, stem, stemLength) == 0 &&
                SDL_strcasecmp(entry->d_name + length - 4, ".bin") == 0)
            {
                dsJoin(path, sizeof(path), dir, entry->d_name);
                dsLog("installer part: %s", path);
                parts++;
            }
        }
        if (d != NULL)
        {
            closedir(d);
        }
    }
#endif
    if (parts == 0)
    {
        dsLog("the installer is a single file (no %s*.bin parts)", stem);
    }
}

// the newest setup_homeworld_emergence*.exe in Downloads, on the Desktop or in the home folder
static int dsFindInstaller(char *out, size_t size)
{
    char dirs[8][DS_PATH], list[DS_PATH * 3];
    int count = 0, i, j;
    dsInstallerScan scan;
#ifdef _WIN32
    const char *home = getenv("USERPROFILE");

    if (dsKnownFolder(&dsFOLDERID_Downloads, dirs[count], DS_PATH))
    {
        count++;
    }
    if (dsKnownFolder(&dsFOLDERID_Desktop, dirs[count], DS_PATH))
    {
        count++;
    }
    if (home != NULL && home[0] != '\0')
    {
        snprintf(dirs[count++], DS_PATH, "%s\\Downloads", home);
        snprintf(dirs[count++], DS_PATH, "%s", home);
    }
#else
    const char *home = getenv("HOME");

    if (dsDownloadsDir(dirs[count], DS_PATH))
    {
        count++;
    }
#ifndef __APPLE__
    if (dsXdgUserDir("DESKTOP", dirs[count], DS_PATH))
    {
        count++;
    }
#endif
    if (home != NULL && home[0] != '\0')
    {
        snprintf(dirs[count++], DS_PATH, "%s/Downloads", home);
        snprintf(dirs[count++], DS_PATH, "%s/Desktop", home);
        snprintf(dirs[count++], DS_PATH, "%s", home);
    }
#ifndef __APPLE__
    if (getenv("APPIMAGE") != NULL && getenv("APPIMAGE")[0] == '/')
    {
        snprintf(dirs[count], DS_PATH, "%s", getenv("APPIMAGE"));       // beside the AppImage
        *(char *)dsBaseName(dirs[count]) = '\0';
        dsTrimSlashes(dirs[count]);
        count++;
    }
#endif
#endif
    if (dsGetcwd(dirs[count], DS_PATH) != NULL)
    {
        count++;
    }
    memset(&scan, 0, sizeof(scan));
    scan.bestTime = -1;
    list[0] = '\0';
    for (i = 0; i < count; i++)
    {
        for (j = 0; j < i; j++)
        {
            if (strcmp(dirs[i], dirs[j]) == 0)
            {
                break;
            }
        }
        if (j < i || !dsIsDir(dirs[i]))
        {
            continue;
        }
        dsListAppend(list, sizeof(list), dirs[i]);
        dsScanInstallers(dirs[i], &scan);
    }
    if (scan.bestTime < 0)
    {
        dsLog("no offline installer (" DS_INSTALLER_PREFIX "*.exe) in %s", list[0] ? list : "(no folders)");
        return 0;
    }
    dsLog("found the offline installer %s", scan.best);
    dsLogParts(scan.best);
    snprintf(out, size, "%s", scan.best);
    return 1;
}

// innoextract: beside the engine (the packages put it there), else on PATH (development)
static void dsInnoextractPath(char *out, size_t size)
{
    char path[DS_PATH];
    char *base;

#ifdef __APPLE__
    {
        char raw[DS_PATH], resolved[PATH_MAX];
        uint32_t length = sizeof(raw);

        if (_NSGetExecutablePath(raw, &length) == 0 && realpath(raw, resolved) != NULL)
        {
            char *slash = strrchr(resolved, '/');

            if (slash != NULL)
            {
                slash[1] = '\0';
                snprintf(path, sizeof(path), "%s" DS_INNOEXTRACT, resolved);
                if (access(path, X_OK) == 0)
                {
                    snprintf(out, size, "%s", path);
                    return;
                }
            }
        }
    }
#endif
    base = SDL_GetBasePath();
    if (base != NULL)
    {
#ifdef _WIN32
        wchar_t wide[DS_PATH];
        char engine[DS_PATH];

        if (MultiByteToWideChar(CP_UTF8, 0, base, -1, wide, DS_PATH) > 0 && dsWideToEngine(wide, engine, sizeof(engine)))
        {
            snprintf(path, sizeof(path), "%s" DS_INNOEXTRACT, engine);
            if (dsFileSize(path, NULL))
            {
                snprintf(out, size, "%s", path);
                SDL_free(base);
                return;
            }
        }
#else
        snprintf(path, sizeof(path), "%s" DS_INNOEXTRACT, base);
        if (access(path, X_OK) == 0 && dsFileSize(path, NULL))
        {
            snprintf(out, size, "%s", path);
            SDL_free(base);
            return;
        }
#ifdef __APPLE__
        snprintf(path, sizeof(path), "%s../MacOS/" DS_INNOEXTRACT, base);
        if (access(path, X_OK) == 0)
        {
            snprintf(out, size, "%s", path);
            SDL_free(base);
            return;
        }
#endif
#endif
        SDL_free(base);
    }
#ifdef __APPLE__
    // a Finder-started app's PATH has no Homebrew
    if (access("/opt/homebrew/bin/innoextract", X_OK) == 0)
    {
        snprintf(out, size, "/opt/homebrew/bin/innoextract");
        return;
    }
    if (access("/usr/local/bin/innoextract", X_OK) == 0)
    {
        snprintf(out, size, "/usr/local/bin/innoextract");
        return;
    }
#endif
    snprintf(out, size, DS_INNOEXTRACT);
}

// the small progress window: the software renderer on the window's own framebuffer, never a GL context
typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    int percent;
} dsProgress;

static void dsProgressDraw(dsProgress *bar, int percent)
{
    char title[96];
    SDL_Rect frame = { 24, 26, 432, 22 }, fill;

    bar->percent = percent;
    if (bar->renderer == NULL)
    {
        return;
    }
    snprintf(title, sizeof(title), "Homeworld Infection: unpacking the game files - %d%%", percent);
    SDL_SetWindowTitle(bar->window, title);
    SDL_SetRenderDrawColor(bar->renderer, 24, 26, 32, 255);
    SDL_RenderClear(bar->renderer);
    SDL_SetRenderDrawColor(bar->renderer, 150, 160, 176, 255);
    SDL_RenderDrawRect(bar->renderer, &frame);
    fill.x = frame.x + 3;
    fill.y = frame.y + 3;
    fill.w = (frame.w - 6) * percent / 100;
    fill.h = frame.h - 6;
    if (fill.w > 0)
    {
        SDL_SetRenderDrawColor(bar->renderer, 64, 150, 220, 255);
        SDL_RenderFillRect(bar->renderer, &fill);
    }
    SDL_RenderPresent(bar->renderer);
}

static void dsProgressOpen(dsProgress *bar)
{
    Uint32 flags = 0;

    memset(bar, 0, sizeof(*bar));
    bar->percent = -1;
    if (!SDL_WasInit(SDL_INIT_VIDEO) && SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
    {
        dsLog("no progress window (%s): the progress is only logged", SDL_GetError());
        return;
    }
    if ((dsScriptOn && !dsScriptShow) || catHiddenWindow)
    {
        flags |= SDL_WINDOW_HIDDEN;                 // HWI-TEST: a test never puts a window on the screen
    }
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
    bar->window = SDL_CreateWindow("Homeworld Infection: unpacking the game files", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, 480, 74, flags);
    if (bar->window != NULL)
    {
        bar->renderer = SDL_CreateRenderer(bar->window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (bar->renderer == NULL)
    {
        dsLog("no progress window (%s): the progress is only logged", SDL_GetError());
        if (bar->window != NULL)
        {
            SDL_DestroyWindow(bar->window);
            bar->window = NULL;
        }
        SDL_ResetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION);
        return;
    }
    dsLog("progress window: the %s video driver, the software renderer%s", SDL_GetCurrentVideoDriver(),
          (flags & SDL_WINDOW_HIDDEN) ? ", hidden (a test)" : "");
    dsProgressDraw(bar, 0);
}

// 1 when the player closed the window
static int dsProgressPump(dsProgress *bar)
{
    SDL_Event event;
    int closed = 0;

    if (bar->window == NULL)
    {
        return 0;
    }
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
        {
            closed = 1;
        }
        else if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                closed = 1;
            }
            else if (event.window.event == SDL_WINDOWEVENT_EXPOSED && bar->percent >= 0)
            {
                dsProgressDraw(bar, bar->percent);
            }
        }
    }
    return closed;
}

// the window destroyed; the video subsystem stays up (a later GL window: NVIDIA + Xwayland)
static void dsProgressClose(dsProgress *bar)
{
    if (bar->renderer != NULL)
    {
        SDL_DestroyRenderer(bar->renderer);
    }
    if (bar->window != NULL)
    {
        SDL_DestroyWindow(bar->window);
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);     // no stray quit for the game's main loop
        SDL_ResetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION);
    }
    memset(bar, 0, sizeof(*bar));
}

// one line of innoextract's output: a progress bar ("[====>   ] 13.2%") or a message
static void dsInnoLine(char *line, dsProgress *bar, int *logged, char *last, size_t lastSize, int *errors)
{
    char clean[1024];
    size_t n = 0;
    char *p = line, *percent;

    // drop the terminal codes ("\033[K")
    while (*p != '\0' && n + 1 < sizeof(clean))
    {
        if (*p == '\033')
        {
            p++;
            if (*p == '[')
            {
                p++;
                while (*p != '\0' && !isalpha((unsigned char)*p))
                {
                    p++;
                }
                if (*p != '\0')
                {
                    p++;
                }
            }
            continue;
        }
        clean[n++] = *p++;
    }
    clean[n] = '\0';
    dsTrimLine(clean);
    p = clean;
    while (*p == ' ')
    {
        p++;
    }
    if (*p == '\0')
    {
        return;
    }
    percent = strchr(p, '%');
    if (*p == '[' && percent != NULL)
    {
        char *digits = percent;
        int value;

        while (digits > p && (isdigit((unsigned char)digits[-1]) || digits[-1] == '.'))
        {
            digits--;
        }
        value = (int)atof(digits);
        if (value >= 0 && value <= 100 && value != bar->percent)
        {
            dsProgressDraw(bar, value);
            if (value >= *logged + 10)
            {
                *logged = value - value % 10;
                dsLog("unpacking: %d%%", value);
            }
        }
        return;
    }
    dsLog("innoextract: %s", p);
    if (strncmp(p, "- \"", 3) == 0)                 // a file name
    {
        return;
    }
    while ((unsigned char)*p >= 0x80 || *p == ' ')  // the tree marks before "error reason: ..."
    {
        p++;
    }
    if (strstr(p, "rror") != NULL && strncmp(p, "Done", 4) != 0)
    {
        // the first error and its reason, for the player's box
        if (*errors == 0)
        {
            snprintf(last, lastSize, "%s", p);
        }
        else if (*errors == 1)
        {
            size_t n = strlen(last);

            snprintf(last + n, lastSize - n, "\n%s", p);
        }
        (*errors)++;
    }
    else if (*errors == 0)
    {
        snprintf(last, lastSize, "%s", p);          // else the latest message
    }
}

// innoextract with its progress in the window; the exit code, -1 = not started
static int dsRunInnoextract(const char *innoextract, const char *installer, const char *outDir, char *last,
                            size_t lastSize, int *cancelled)
{
    const char *argv[] = { innoextract, "--gog", "--exclude-temp", "--language", "english", "--progress=1",
                           "--color=0", "--output-dir", outDir, "--", installer, NULL };
    dsProgress bar;
    dsProc proc;
    char buf[4096], line[1024];
    size_t length = 0;
    int logged = -10, errors = 0, got, rc, i;

    *cancelled = 0;
    last[0] = '\0';
    dsLog("running %s --gog --exclude-temp --language english --progress=1 --color=0 --output-dir \"%s\" -- \"%s\"",
          innoextract, outDir, installer);
    if (!dsProcStart(&proc, argv))
    {
        snprintf(last, lastSize, "innoextract could not be started (%s)", innoextract);
        return -1;
    }
    dsProgressOpen(&bar);
    while ((got = dsProcRead(&proc, buf, sizeof(buf), 50)) >= 0)
    {
        for (i = 0; i < got; i++)
        {
            if (buf[i] == '\r' || buf[i] == '\n')
            {
                line[length] = '\0';
                dsInnoLine(line, &bar, &logged, last, lastSize, &errors);
                length = 0;
            }
            else if (length + 1 < sizeof(line))
            {
                line[length++] = buf[i];
            }
        }
        if (!*cancelled && dsProgressPump(&bar))
        {
            dsLog("the progress window was closed: stopping innoextract");
            dsProcKill(&proc);
            *cancelled = 1;
        }
    }
    line[length] = '\0';
    dsInnoLine(line, &bar, &logged, last, lastSize, &errors);
    rc = dsProcWait(&proc);
    dsProgressClose(&bar);
    dsLog("innoextract exit code %d", rc);
    return rc;
}

// the installer is GOG's Homeworld: Emergence (innoextract --gog-game-id); why = the reason when not
static int dsInstallerCheck(const char *innoextract, const char *installer, char *why, size_t whySize)
{
    const char *argv[] = { innoextract, "--gog-game-id", "--", installer, NULL };
    char out[2048], err[1024], first[256], *id;
    int rc = dsRunCapture(argv, out, sizeof(out), err, sizeof(err));

    if (rc == -1)
    {
        snprintf(why, whySize, "The unpacking program (innoextract) was not found.\n"
                               "It belongs beside the game's program (%s).", innoextract);
        return 0;
    }
    id = strstr(out, "GOG.com game ID is ");
    if (rc != 0)
    {
        char lines[1024], *errorLine;

        snprintf(lines, sizeof(lines), "%s%s", err, out);
        errorLine = lines;
        while (*errorLine == '\n' || *errorLine == '\r')
        {
            errorLine++;
        }
        dsFirstLine(errorLine, first, sizeof(first));
        dsLog("innoextract --gog-game-id: exit %d: %s", rc, first);
        snprintf(why, whySize, "innoextract cannot read this file (exit code %d):\n%s\n\n"
                               "It may be incomplete or damaged: download it again from GOG.com.", rc, first);
        return 0;
    }
    if (id != NULL)
    {
        dsFirstLine(id, first, sizeof(first));
        dsLog("innoextract --gog-game-id: %s", first);
        if (strncmp(id + strlen("GOG.com game ID is "), DS_GOG_GAME_ID, strlen(DS_GOG_GAME_ID)) != 0)
        {
            snprintf(why, whySize, "This is the installer of another GOG game (%s).\n"
                                   "Homeworld: Emergence's is GOG game " DS_GOG_GAME_ID ".", first);
            return 0;
        }
        return 1;
    }
    dsLog("innoextract --gog-game-id: no GOG game id in this installer: trying it anyway");
    return 1;
}

// the installer unpacked into the per-user GameData folder; 1 = done (its path in result), else why
static int dsUnpack(const char *installer, char *result, size_t resultSize, char *why, size_t whySize)
{
    char base[DS_PATH], gameData[DS_PATH], temp[DS_PATH], app[DS_PATH], innoextract[DS_PATH], last[512];
    char missing[256];
    long long installerSize = 0, freeBytes;
    Uint32 start = SDL_GetTicks();
    dsTreeSize total;
    int rc, cancelled = 0;

    why[0] = '\0';
    base[0] = '\0';
    dsLog("unpacking the offline installer %s", installer);
    if (!dsFileSize(installer, &installerSize))
    {
        snprintf(why, whySize, "The installer was not found:\n%s", installer);
        return 0;
    }
    dsLog("installer: %lld bytes", installerSize);
    if (SDL_strncasecmp(dsBaseName(installer), "GOG_Galaxy", 10) == 0)
    {
        snprintf(why, whySize, "This is GOG Galaxy's small download helper, not the offline installer.\n"
                               "On GOG.com, under Homeworld: Emergence, download the offline backup installer\n"
                               "(setup_homeworld_emergence_....exe) instead.");
        return 0;
    }
    if (!dsUserDataDir(base, sizeof(base)) || !dsMakeDirs(base))
    {
        snprintf(why, whySize, "No folder for the game files could be created (%s).",
                 base[0] ? base : "LOCALAPPDATA / HOME is not set");
        return 0;
    }
    dsJoin(gameData, sizeof(gameData), base, "GameData");
    dsJoin(temp, sizeof(temp), base, "unpack.tmp");
    freeBytes = dsFreeBytes(base);
    dsLog("free disk space in %s: %lld MB (about %lld MB needed)", base, freeBytes / (1024 * 1024),
          DS_FREE_NEEDED / (1024 * 1024));
    if (freeBytes >= 0 && freeBytes < DS_FREE_NEEDED)
    {
        snprintf(why, whySize, "Not enough free disk space. The game files need about %lld MB in\n%s\n"
                               "and only %lld MB are free.", DS_FREE_NEEDED / (1024 * 1024), base,
                 freeBytes / (1024 * 1024));
        return 0;
    }
    dsInnoextractPath(innoextract, sizeof(innoextract));
    dsLog("innoextract: %s", innoextract);
    if (!dsInstallerCheck(innoextract, installer, why, whySize))
    {
        return 0;
    }
    if (dsIsDir(temp))
    {
        dsLog("removing the temp folder of an earlier unpack: %s", temp);
        dsRemoveTree(temp);
    }
    if (!dsMakeDirs(temp))
    {
        snprintf(why, whySize, "The temp folder could not be created:\n%s", temp);
        return 0;
    }
    rc = dsRunInnoextract(innoextract, installer, temp, last, sizeof(last), &cancelled);
    if (cancelled || rc != 0)
    {
        dsRemoveTree(temp);
        if (cancelled)
        {
            snprintf(why, whySize, "Unpacking was stopped.");
        }
        else if (rc < 0)
        {
            snprintf(why, whySize, "The unpacking program (innoextract) could not be started:\n%s", innoextract);
        }
        else
        {
            snprintf(why, whySize, "innoextract stopped with exit code %d:\n%s\n\n"
                                   "The installer may be incomplete or damaged: download it again from GOG.com.", rc,
                     last[0] ? last : "(no message)");
        }
        return 0;
    }
    dsJoin(app, sizeof(app), temp, "app");          // innoextract puts Inno's {app} in app/
    if (!dsValidate(app, missing, sizeof(missing), 1))
    {
        dsRemoveTree(temp);
        snprintf(why, whySize, "The installer did not hold the game files (missing: %s).\n"
                               "Is it the installer of Homeworld: Emergence?", missing);
        return 0;
    }
    if (dsIsDir(gameData) && dsRmdir(gameData) != 0)
    {
        // an earlier copy that is not whole: kept, renamed, never deleted
        char old[DS_PATH], stamp[32];
        time_t now = time(NULL);

        strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", localtime(&now));
        snprintf(old, sizeof(old), "%s.old-%s", gameData, stamp);
        if (rename(gameData, old) != 0)
        {
            dsRemoveTree(temp);
            snprintf(why, whySize, "The folder\n%s\nis in the way and cannot be renamed (%s).", gameData, strerror(errno));
            return 0;
        }
        dsLog("an earlier, incomplete GameData was renamed to %s", old);
    }
    if (rename(app, gameData) != 0)
    {
        snprintf(why, whySize, "The game files could not be moved into\n%s\n(%s).", gameData, strerror(errno));
        dsRemoveTree(temp);
        return 0;
    }
    dsRemoveTree(temp);
    if (!dsValidate(gameData, missing, sizeof(missing), 1))
    {
        snprintf(why, whySize, "The unpacked game files are not complete (missing: %s).", missing);
        return 0;
    }
    memset(&total, 0, sizeof(total));
    dsWalk(gameData, dsCountOne, &total);
    dsLog("unpacked %d files, %lld bytes into %s in %.1f s", total.files, total.bytes, gameData,
          (SDL_GetTicks() - start) / 1000.0);
    snprintf(result, resultSize, "%s", gameData);
    return 1;
}

/*-----------------------------------------------------------------------------
    The first-run flow
-----------------------------------------------------------------------------*/
static char *dsUse(const char *dir, char *path, int pathSize, int save)
{
    static char error[DS_PATH + 128];

    if ((int)strlen(dir) >= pathSize)
    {
        snprintf(error, sizeof(error), "datasetup: the game folder's path is longer than %d characters: %s",
                 pathSize - 1, dir);
        return error;
    }
    snprintf(path, (size_t)pathSize, "%s", dir);
    dsTrimSlashes(path);
    if (save)
    {
        dsSavedPathWrite(path);
    }
    dsLog("the game files: %s", path);
    return NULL;
}

static char *dsQuitNow(void)
{
    dsQuit = 1;
    dsLog("the player chose Quit");
    return "datasetup: the player chose Quit";
}

static char *dsBoxFailed(void)
{
    return "datasetup: no game files, and the dialog that asks for them could not be shown "
           "(set HW_Data to the GOG game folder)";
}

// the folder chosen, or the GOG folder inside it ("Homeworld Emergence", innoextract's "app")
static int dsResolveFolder(const char *chosen, char *out, size_t size, char *missing, size_t missingSize)
{
    static const char *const inside[] = { "Homeworld Emergence", "app", NULL };
    char path[DS_PATH];
    int i;

    if (dsValidate(chosen, missing, missingSize, 1))
    {
        snprintf(out, size, "%s", chosen);
        return 1;
    }
    for (i = 0; inside[i] != NULL; i++)
    {
        dsJoin(path, sizeof(path), chosen, inside[i]);
        if (dsIsDir(path) && dsValidate(path, NULL, 0, 1))
        {
            snprintf(out, size, "%s", path);
            return 1;
        }
    }
    return 0;
}

static int dsUnpackFailed(const char *installer, const char *why)
{
    char text[3072];

    snprintf(text, sizeof(text), "The game files could not be unpacked from\n%s\n\n%s", installer, why);
    dsLogText("unpack failed: ", why);
    return dsMessage(SDL_MESSAGEBOX_ERROR, text);
}

// no file chooser: the player copies the files into GameData with the file manager. 1 = there (dir), 0 = quit,
//      DS_BOX_FAILED
static int dsCopyByHand(char *dir, size_t size)
{
    static const char *const buttons[] = { "Open that folder", "Retry", "Quit" };
    static const char *const keys[] = { "open", "retry", "quit" };
    char gameData[DS_PATH], missing[256], text[3072];

    if (!dsGameDataDir(gameData, sizeof(gameData)))
    {
        dsLog("no per-user folder (HOME is not set): nowhere to copy the files to");
        return 0;
    }
    missing[0] = '\0';
    for (;;)
    {
        int choice;

        snprintf(text, sizeof(text),
                 "Homeworld Infection could not open a file chooser on this computer\n"
                 "(it uses zenity or kdialog).\n\n"
                 "Copy everything from your GOG Homeworld: Emergence folder\n"
                 "(cataclysm.big, update.big, the Movies folder and the rest) into:\n\n%s\n\n"
                 "then press Retry.%s%s", gameData, missing[0] ? "\n\nStill missing: " : "", missing);
        choice = dsBox(SDL_MESSAGEBOX_WARNING, text, buttons, keys, 3, 1, 2);
        if (choice == DS_BOX_FAILED)
        {
            return DS_BOX_FAILED;
        }
        if (choice < 0 || choice == 2)
        {
            return 0;
        }
        if (choice == 0)
        {
            dsMakeDirs(gameData);
            dsOpenFolder(gameData);
            continue;
        }
        if (dsValidate(gameData, missing, sizeof(missing), 1))
        {
            snprintf(dir, size, "%s", gameData);
            return 1;
        }
    }
}

static char *dsAsk(char *path, int pathSize)
{
    static const char *const buttons[] = { "Choose the GOG game folder", "Choose the offline installer", "Quit" };
    static const char *const keys[] = { "folder", "installer", "quit" };
    static const char *const question =
        "Homeworld Infection needs the game files of Homeworld: Emergence,\n"
        "from your copy of the game on GOG.com.\n\n"
        "Choose one of these:\n"
        "  - the folder where GOG installed the game (it holds cataclysm.big), or\n"
        "  - the offline installer you downloaded from GOG\n"
        "    (setup_homeworld_emergence_....exe): its files are unpacked once.\n\n"
        "Nothing in the GOG folder is changed.";
    char chosen[DS_PATH], dir[DS_PATH], missing[256], why[2048], text[3072];

    for (;;)
    {
        int choice = dsBox(SDL_MESSAGEBOX_INFORMATION, question, buttons, keys, 3, 0, 2);
        int got;

        if (choice == DS_BOX_FAILED)
        {
            return dsBoxFailed();
        }
        if (choice < 0 || choice == 2)
        {
            return dsQuitNow();
        }
        if (dsScriptOn && dsScriptArg[0] != '\0')
        {
            snprintf(chosen, sizeof(chosen), "%s", dsScriptArg);     // HWI-TEST: the scripted pick
            dsLog("[test] picked %s", chosen);
            got = 1;
        }
        else
        {
            got = (choice == 0) ? dsPickFolder(chosen, sizeof(chosen)) : dsPickInstaller(chosen, sizeof(chosen));
        }
        if (got == 0)
        {
            continue;                               // cancelled: the question again
        }
        if (got < 0)
        {
            got = dsCopyByHand(dir, sizeof(dir));
            if (got == DS_BOX_FAILED)
            {
                return dsBoxFailed();
            }
            if (got == 0)
            {
                return dsQuitNow();
            }
            return dsUse(dir, path, pathSize, 1);
        }
        if (choice == 0)
        {
            if (dsResolveFolder(chosen, dir, sizeof(dir), missing, sizeof(missing)))
            {
                return dsUse(dir, path, pathSize, 1);
            }
            snprintf(text, sizeof(text), "That folder does not hold the game files:\n\n%s\n\nMissing: %s\n\n"
                                         "Choose the folder where GOG installed Homeworld: Emergence.\n"
                                         "It holds cataclysm.big, update.big and the Movies folder.", chosen, missing);
            if (dsMessage(SDL_MESSAGEBOX_ERROR, text) == DS_BOX_FAILED)
            {
                return dsBoxFailed();
            }
        }
        else
        {
            if (dsUnpack(chosen, dir, sizeof(dir), why, sizeof(why)))
            {
                return dsUse(dir, path, pathSize, 1);
            }
            if (dsUnpackFailed(chosen, why) == DS_BOX_FAILED)
            {
                return dsBoxFailed();
            }
        }
    }
}

/*-----------------------------------------------------------------------------
    Name        : dsGameDataFind
    Description : HWI-FEATURE(data-setup): the game data folder when HW_Data is not set (utyGameSystemsPreInit)
    Inputs      : path, pathSize - where the folder goes; optionsRow - the options file's HomeworldDataPath
    Return      : NULL on success; else the reason (dsQuitChosen: the player quit)
----------------------------------------------------------------------------*/
char *dsGameDataFind(char *path, int pathSize, const char *optionsRow)
{
    static char error[DS_PATH * 2 + 256];
    char dir[DS_PATH], file[DS_PATH], installer[DS_PATH], why[2048], text[3072];
    const char *noDialog;

    dsQuit = 0;
    dsLog("HW_Data is not set: looking for the game files");
    if (dsScriptOn)
    {
        dsLog("[test] scripted answers: %s", dsScriptBuf);
    }

    // the working directory (the old Windows zip layout: the engine in the game folder)
    if (dsGetcwd(dir, sizeof(dir)) != NULL)
    {
        dsJoin(file, sizeof(file), dir, "cataclysm.big");
        if (dsFileSize(file, NULL))
        {
            dsLog("the working directory holds cataclysm.big: using it");
            dsValidate(dir, NULL, 0, 1);
            return dsUse(dir, path, pathSize, 0);
        }
        dsLog("the working directory (%s) has no cataclysm.big", dir);
    }

    // the path saved by an earlier start
    if (dsSavedPathFile(file, sizeof(file)))
    {
        if (dsSavedPathRead(file, dir, sizeof(dir)))
        {
            if (dsValidate(dir, NULL, 0, 1))
            {
                dsLog("using the path saved in %s", file);
                return dsUse(dir, path, pathSize, 0);
            }
            dsLog("the path saved in %s no longer holds the game files", file);
        }
        else
        {
            dsLog("no saved path (%s)", file);
        }
    }

    // GoK's own saved path: the options file's HomeworldDataPath row (every run writes it)
    if (optionsRow != NULL && optionsRow[0] != '\0')
    {
        snprintf(dir, sizeof(dir), "%s", optionsRow);
        dsTrimSlashes(dir);
        if (dsValidate(dir, NULL, 0, 1))
        {
            dsLog("using the options file's HomeworldDataPath");
            return dsUse(dir, path, pathSize, 1);
        }
        dsLog("the options file's HomeworldDataPath (%s) no longer holds the game files: ignored", dir);
    }

    // an installed copy
    if (dsAutoDetect(dir, sizeof(dir)) > 0)
    {
        return dsUse(dir, path, pathSize, 1);
    }

    // the offline installer in Downloads, on the Desktop or in the home folder
    noDialog = dsNoDialogReason();
    if (dsFindInstaller(installer, sizeof(installer)))
    {
        static const char *const buttons[] = { "Unpack", "Choose something else", "Quit" };
        static const char *const keys[] = { "unpack", "other", "quit" };
        int choice;

        if (noDialog != NULL)
        {
            snprintf(error, sizeof(error), "datasetup: no game files; the offline installer %s was found, but this "
                     "run cannot ask before unpacking it (%s). Set HW_Data to the GOG game folder.", installer, noDialog);
            dsLog("%s", error + strlen("datasetup: "));
            return error;
        }
        dsGameDataDir(dir, sizeof(dir));
        snprintf(text, sizeof(text), "Homeworld Infection needs the game files of Homeworld: Emergence.\n"
                                     "It found your GOG offline installer:\n\n%s\n\n"
                                     "Unpack the game files from it now? They take about 400 MB in:\n\n%s",
                 installer, dir);
        choice = dsBox(SDL_MESSAGEBOX_INFORMATION, text, buttons, keys, 3, 0, 2);
        if (choice == DS_BOX_FAILED)
        {
            return dsBoxFailed();
        }
        if (choice < 0 || choice == 2)
        {
            return dsQuitNow();
        }
        if (choice == 0)
        {
            if (dsUnpack(installer, dir, sizeof(dir), why, sizeof(why)))
            {
                return dsUse(dir, path, pathSize, 1);
            }
            if (dsUnpackFailed(installer, why) == DS_BOX_FAILED)
            {
                return dsBoxFailed();
            }
        }
    }

    if (noDialog != NULL)
    {
        snprintf(error, sizeof(error), "datasetup: no game files found, and this run cannot ask for them (%s). Set "
                 "HW_Data to the GOG game folder (the folder that holds cataclysm.big).", noDialog);
        dsLog("%s", error + strlen("datasetup: "));
        return error;
    }

    // ask: the GOG folder or the offline installer
    return dsAsk(path, pathSize);
}
