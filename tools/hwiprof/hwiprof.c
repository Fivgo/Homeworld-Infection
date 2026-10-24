/* hwiprof: a wall-clock sampling profiler for hwi-engine, loaded with LD_PRELOAD (HWI-TEST, M3-R2 mesh-batching).
 *
 * The machine has no perf / valgrind; this shim samples the MAIN thread on a CLOCK_MONOTONIC timer (wall time, so a
 * wait in glFinish or the swap is sampled too) and records each sample's call stack (the interrupted PC plus
 * backtrace()'s frames). At exit it writes HWIPROF_OUT (binary: "HWIPROF1", then per sample u32 depth + depth u64
 * addresses) and HWIPROF_OUT.maps (/proc/self/maps). tools/hwiprof/hwiprof_report.py symbolises it (addr2line).
 *
 * Build: cc -O2 -shared -fPIC -o <scratch>/libhwiprof.so tools/hwiprof/hwiprof.c
 * Use (through the harness only, docs/CONSTRAINTS.md item 5): a wrapper script given to m2_harness.sh --bin that
 * exports LD_PRELOAD=<scratch>/libhwiprof.so HWIPROF_OUT=<file> [HWIPROF_US=500] and execs the engine binary.
 */
#define _GNU_SOURCE
#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ucontext.h>
#include <sys/syscall.h>
#include <stdatomic.h>

#define HWIPROF_MAXF 64
#define HWIPROF_CAP  (1L << 19)

static uint64_t *prof_frames;          /* HWIPROF_CAP * HWIPROF_MAXF */
static uint32_t *prof_depth;
static atomic_long prof_n;
static timer_t prof_timer;
static int prof_on;
static char prof_out[4096];

static void prof_handler(int sig, siginfo_t *si, void *ucv)
{
    ucontext_t *uc = (ucontext_t *)ucv;
    void *bt[HWIPROF_MAXF + 4];
    long i;
    int d, k, o;
    (void)sig; (void)si;

    i = atomic_fetch_add(&prof_n, 1);
    if (i >= HWIPROF_CAP) return;
    d = backtrace(bt, HWIPROF_MAXF + 4);
    o = 0;
    prof_frames[i * HWIPROF_MAXF + o++] = (uint64_t)uc->uc_mcontext.gregs[REG_RIP];
    /* bt[0] = this handler, bt[1] = the signal trampoline, bt[2] = the interrupted PC again: skip them */
    for (k = 3; k < d && o < HWIPROF_MAXF; k++)
        prof_frames[i * HWIPROF_MAXF + o++] = (uint64_t)bt[k];
    prof_depth[i] = (uint32_t)o;
}

static void prof_write(void)
{
    FILE *f;
    long n, i;
    char maps[4200], buf[65536];
    size_t r;
    FILE *in;

    if (!prof_on) return;
    prof_on = 0;
    timer_delete(prof_timer);
    n = atomic_load(&prof_n);
    if (n > HWIPROF_CAP) n = HWIPROF_CAP;
    f = fopen(prof_out, "wb");
    if (f == NULL) return;
    fwrite("HWIPROF1", 1, 8, f);
    for (i = 0; i < n; i++)
    {
        fwrite(&prof_depth[i], 4, 1, f);
        fwrite(&prof_frames[i * HWIPROF_MAXF], 8, prof_depth[i], f);
    }
    fclose(f);
    snprintf(maps, sizeof(maps), "%s.maps", prof_out);
    in = fopen("/proc/self/maps", "r");
    f = fopen(maps, "w");
    if (in != NULL && f != NULL)
        while ((r = fread(buf, 1, sizeof(buf), in)) > 0) fwrite(buf, 1, r, f);
    if (in) fclose(in);
    if (f) fclose(f);
    fprintf(stderr, "hwiprof: %ld samples written to %s\n", n, prof_out);
}

static void prof_sigterm(int sig)
{
    prof_write();
    signal(sig, SIG_DFL);
    raise(sig);
}

__attribute__((constructor)) static void prof_init(void)
{
    const char *out = getenv("HWIPROF_OUT");
    const char *us = getenv("HWIPROF_US");
    struct sigaction sa;
    struct sigevent sev;
    struct itimerspec its;
    void *warm[4];
    long period = (us != NULL) ? atol(us) : 500;

    if (out == NULL || prof_on) return;
    unsetenv("LD_PRELOAD");                    /* not into children */
    snprintf(prof_out, sizeof(prof_out), "%s", out);
    prof_frames = calloc((size_t)HWIPROF_CAP * HWIPROF_MAXF, 8);
    prof_depth = calloc((size_t)HWIPROF_CAP, 4);
    if (prof_frames == NULL || prof_depth == NULL) return;
    backtrace(warm, 4);                        /* load libgcc's unwinder before the first signal */
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = prof_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGPROF;
    sev._sigev_un._tid = (pid_t)syscall(SYS_gettid);
    if (timer_create(CLOCK_MONOTONIC, &sev, &prof_timer) != 0) return;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = period * 1000;
    its.it_value = its.it_interval;
    timer_settime(prof_timer, 0, &its, NULL);
    prof_on = 1;
    atexit(prof_write);
    signal(SIGTERM, prof_sigterm);
}

__attribute__((destructor)) static void prof_fini(void)
{
    prof_write();
}
