/*
 * hwi_oracle_loader.c - starts cataclysm.exe suspended from its own directory, loads hwi_oracle.dll into it, then lets
 * it run (CET's loader pattern, docs/cet-knowledge.md section 8, with the two fixes the oracle design asks for in
 * docs/m2-checksum-oracle.md 3.1: the remote LoadLibraryA result is checked and the child is terminated if the DLL did
 * not load, and the forwarded arguments are passed through unchanged, quotes included).
 *
 * Usage (from the game directory, through Wine/Proton; tools/run_game.sh --exe hwi_oracle_loader.exe -- ARGS):
 *   hwi_oracle_loader.exe [game arguments...]     e.g. /logOn
 * The DLL reads its options from HWI_ORACLE (hwi_oracle.c). Exit code: the game's, or 10x on a loader failure.
 * Freestanding like the DLL (kernel32 only). NEVER run by the build; the user runs the first live session.
 */
#include <stddef.h>
#include <stdint.h>

typedef int BOOL;
typedef uint32_t DWORD;
typedef void *HANDLE;
typedef void *HMODULE;
#define WINAPI __stdcall
#define DLLIMPORT __declspec(dllimport)
#define CREATE_SUSPENDED 0x4u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define MEM_RELEASE 0x8000u
#define PAGE_READWRITE 0x4u
#define INFINITE 0xFFFFFFFFu
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 0x1u
#define CREATE_ALWAYS 2u
#define FILE_ATTRIBUTE_NORMAL 0x80u
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)

typedef struct
{
    DWORD cb;
    char *lpReserved, *lpDesktop, *lpTitle;
    DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    uint16_t wShowWindow, cbReserved2;
    uint8_t *lpReserved2;
    HANDLE hStdInput, hStdOutput, hStdError;
} STARTUPINFOA;

typedef struct
{
    HANDLE hProcess, hThread;
    DWORD dwProcessId, dwThreadId;
} PROCESS_INFORMATION;

typedef DWORD (WINAPI *THREAD_START)(void *);

DLLIMPORT BOOL WINAPI CreateProcessA(const char *, char *, void *, void *, BOOL, DWORD, void *, const char *,
                                     STARTUPINFOA *, PROCESS_INFORMATION *);
DLLIMPORT void *WINAPI VirtualAllocEx(HANDLE, void *, size_t, DWORD, DWORD);
DLLIMPORT BOOL WINAPI VirtualFreeEx(HANDLE, void *, size_t, DWORD);
DLLIMPORT BOOL WINAPI WriteProcessMemory(HANDLE, void *, const void *, size_t, size_t *);
DLLIMPORT HANDLE WINAPI CreateRemoteThread(HANDLE, void *, size_t, THREAD_START, void *, DWORD, DWORD *);
DLLIMPORT DWORD WINAPI WaitForSingleObject(HANDLE, DWORD);
DLLIMPORT BOOL WINAPI GetExitCodeThread(HANDLE, DWORD *);
DLLIMPORT BOOL WINAPI GetExitCodeProcess(HANDLE, DWORD *);
DLLIMPORT DWORD WINAPI ResumeThread(HANDLE);
DLLIMPORT BOOL WINAPI TerminateProcess(HANDLE, unsigned int);
DLLIMPORT BOOL WINAPI CloseHandle(HANDLE);
DLLIMPORT HMODULE WINAPI GetModuleHandleA(const char *);
DLLIMPORT void *WINAPI GetProcAddress(HMODULE, const char *);
DLLIMPORT DWORD WINAPI GetModuleFileNameA(HMODULE, char *, DWORD);
DLLIMPORT char *WINAPI GetCommandLineA(void);
DLLIMPORT void WINAPI ExitProcess(unsigned int);
DLLIMPORT HANDLE WINAPI CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
DLLIMPORT BOOL WINAPI WriteFile(HANDLE, const void *, DWORD, DWORD *, void *);
DLLIMPORT void WINAPI OutputDebugStringA(const char *);

void *memset(void *d, int c, size_t n)
{
    unsigned char *p = (unsigned char *)d;
    while (n--) *p++ = (unsigned char)c;
    return d;
}

static size_t str_len(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

static void str_cat(char *d, const char *s, size_t cap)
{
    size_t n = str_len(d), i = 0;
    while (s[i] && n + 1 < cap) d[n++] = s[i++];
    d[n] = '\0';
}

static char dir[600], exe[700], dll[700], cmd[4096], logname[700];

static void log_msg(const char *a, const char *b)
{
    static char line[1024];
    HANDLE f;
    DWORD w;
    line[0] = '\0';
    str_cat(line, "hwi_oracle_loader: ", sizeof(line));
    str_cat(line, a, sizeof(line));
    if (b) str_cat(line, b, sizeof(line));
    str_cat(line, "\r\n", sizeof(line));
    OutputDebugStringA(line);
    f = CreateFileA(logname, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f != INVALID_HANDLE_VALUE)
    {
        WriteFile(f, line, (DWORD)str_len(line), &w, NULL);
        CloseHandle(f);
    }
}

/* the command line after argv[0], untouched (quotes and all) */
static const char *args_after_program(const char *c)
{
    if (*c == '"')
    {
        c++;
        while (*c && *c != '"') c++;
        if (*c == '"') c++;
    }
    else
    {
        while (*c && *c != ' ' && *c != '\t') c++;
    }
    while (*c == ' ' || *c == '\t') c++;
    return c;
}

static unsigned int run(void)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    void *remote, *loadLibrary;
    HANDLE thread;
    DWORD n, code = 0;
    size_t len, written = 0;

    n = GetModuleFileNameA(NULL, dir, sizeof(dir));
    if (n == 0 || n >= sizeof(dir)) return 101;
    while (n > 0 && dir[n - 1] != '\\' && dir[n - 1] != '/') n--;
    dir[n] = '\0';                                  /* keeps the trailing separator */
    exe[0] = dll[0] = cmd[0] = logname[0] = '\0';
    str_cat(exe, dir, sizeof(exe)); str_cat(exe, "cataclysm.exe", sizeof(exe));
    str_cat(dll, dir, sizeof(dll)); str_cat(dll, "hwi_oracle.dll", sizeof(dll));
    str_cat(logname, dir, sizeof(logname)); str_cat(logname, "hwi_oracle_loader.log", sizeof(logname));
    str_cat(cmd, "\"", sizeof(cmd)); str_cat(cmd, exe, sizeof(cmd)); str_cat(cmd, "\" ", sizeof(cmd));
    str_cat(cmd, args_after_program(GetCommandLineA()), sizeof(cmd));

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(exe, cmd, NULL, NULL, 0, CREATE_SUSPENDED, NULL, dir, &si, &pi))
    {
        log_msg("CreateProcessA failed for ", exe);
        return 102;
    }

    len = str_len(dll) + 1;
    remote = VirtualAllocEx(pi.hProcess, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    loadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    if (remote == NULL || loadLibrary == NULL || !WriteProcessMemory(pi.hProcess, remote, dll, len, &written) || written != len)
    {
        log_msg("could not place the DLL path in the game process", NULL);
        TerminateProcess(pi.hProcess, 1);
        return 103;
    }
    thread = CreateRemoteThread(pi.hProcess, NULL, 0, (THREAD_START)loadLibrary, remote, 0, NULL);
    if (thread == NULL)
    {
        log_msg("CreateRemoteThread failed", NULL);
        TerminateProcess(pi.hProcess, 1);
        return 104;
    }
    WaitForSingleObject(thread, INFINITE);
    GetExitCodeThread(thread, &code);               /* LoadLibraryA's HMODULE, truncated: 0 means it failed */
    CloseHandle(thread);
    VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);
    if (code == 0)
    {
        log_msg("LoadLibraryA failed in the game process for ", dll);
        TerminateProcess(pi.hProcess, 1);
        return 105;
    }

    log_msg("hwi_oracle.dll loaded; game resumed: ", cmd);
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

void WINAPI mainCRTStartup(void)
{
    ExitProcess(run());
}
