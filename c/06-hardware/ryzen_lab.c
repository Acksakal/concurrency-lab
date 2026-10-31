/*
 * ryzen_lab.c - three small experiments that show what POSIX threads
 * MAY and MAY NOT do on a real multi-core machine.
 *
 *   1. TIMELINE  Where and when do threads actually run?
 *                (free on all CPUs, then pinned to ONE CPU)
 *   2. RACE      count++ from two threads: without a mutex, with a mutex,
 *                and without a mutex but on one CPU.
 *   3. SB        "Store buffering" litmus test: an outcome that NO
 *                one-step-at-a-time interleaving can explain.
 *
 * Language: C90 + GNU extensions (gnu90). Uses POSIX threads plus three
 * GNU/Linux extensions that Cygwin also provides: sched_getcpu(),
 * sched_getaffinity()/sched_setaffinity() and the CPU_SET macros.
 *
 * Build (Cygwin 3.6 or Linux):
 *   gcc -std=gnu90 -O2 -Wall -Wextra -pedantic -pthread ryzen_lab.c -o lab
 * Run:
 *   ./lab > out.txt              (defaults)
 *   ./lab 5000000 2000000 > out.txt   (race N, store-buffer iterations)
 * Then load out.txt into docs/thread-ryzen.html.
 *
 * WARNING: experiments 2 and 3 contain DELIBERATE data races. POSIX says an
 * application shall not do this, and in C11 terms it is undefined behaviour.
 * We do it on purpose, with volatile and compiler barriers so the compiler
 * emits exactly the loads and stores we wrote, to watch what the hardware
 * does. Never write production code like this.
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORKERS  8
#define MAX_SEGS     4096
#define GAP_US       50.0      /* a pause longer than this = "was taken off the CPU" */

/* Compiler barrier: the compiler may not move memory accesses across it.
   It emits no instruction, so the CPU itself is still free to reorder. */
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")
/* Full hardware barrier (MFENCE on x86): also stops the CPU reordering. */
#define FULL_BARRIER()     __sync_synchronize()

static double g_t0;            /* time zero of the current experiment */

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static double rel(double t) { return t - g_t0; }

/* ---------------- CPU affinity helpers ---------------- */

static cpu_set_t g_saved_mask;
static int g_pinned = 0;       /* 1 while the process is pinned to one CPU */

/* Pin the calling thread (and every thread it creates afterwards) to one
   CPU: the highest-numbered CPU we are currently allowed to use. */
static int pin_to_one_cpu(void)
{
    cpu_set_t one;
    int cpu, chosen = -1;

    if (sched_getaffinity(0, sizeof g_saved_mask, &g_saved_mask) != 0)
        return -1;
    for (cpu = 0; cpu < CPU_SETSIZE; cpu++)
        if (CPU_ISSET(cpu, &g_saved_mask))
            chosen = cpu;
    if (chosen < 0)
        return -1;
    CPU_ZERO(&one);
    CPU_SET(chosen, &one);
    if (sched_setaffinity(0, sizeof one, &one) != 0)
        return -1;
    g_pinned = 1;
    return chosen;
}

static void unpin(void)
{
    sched_setaffinity(0, sizeof g_saved_mask, &g_saved_mask);
    g_pinned = 0;
}

/* Busy-wait politely: when everyone shares one CPU, spinning without
   yielding would just burn our whole time slice. */
static void spin_pause(unsigned long *spins)
{
    (*spins)++;
    if (g_pinned || (*spins & 1023UL) == 0)
        sched_yield();
}

/* =====================================================================
 * 1. TIMELINE
 * Each worker runs for a fixed wall-clock window and keeps asking
 * "which CPU am I on, and what time is it?". A new segment starts when
 * the CPU changes or when time jumps by more than GAP_US (the thread
 * was not running in between).
 * ===================================================================== */

struct seg { double a, b; int cpu; };

struct tl_worker {
    int id;
    pthread_t tid;
    double t_create_call, t_create_ret;   /* recorded by main */
    double t_start, t_end;                /* recorded by the worker itself */
    int nseg;
    struct seg segs[MAX_SEGS];
};

static struct tl_worker g_w[MAX_WORKERS];
static double g_deadline;

static void *tl_worker_fn(void *arg)
{
    struct tl_worker *w = (struct tl_worker *)arg;
    double t, last, a;
    int cpu, cur;
    volatile unsigned long work = 0;

    w->t_start = now_us();                /* first thing the thread does */
    cur = sched_getcpu();
    a = last = w->t_start;
    w->nseg = 0;

    while ((t = now_us()) < g_deadline) {
        cpu = sched_getcpu();
        if (cpu != cur || t - last > GAP_US) {
            if (w->nseg < MAX_SEGS) {
                w->segs[w->nseg].a = a;
                w->segs[w->nseg].b = last;
                w->segs[w->nseg].cpu = cur;
                w->nseg++;
            }
            a = t;
            cur = cpu;
        }
        last = t;
        work++;                           /* pretend to do something */
    }
    if (w->nseg < MAX_SEGS) {
        w->segs[w->nseg].a = a;
        w->segs[w->nseg].b = last;
        w->segs[w->nseg].cpu = cur;
        w->nseg++;
    }
    w->t_end = now_us();
    return NULL;
}

static void timeline(const char *name, int nworkers, double window_ms, int pin)
{
    int i, k, pinned_cpu = -1;
    double tj0, tj1;

    if (pin) {
        pinned_cpu = pin_to_one_cpu();
        if (pinned_cpu < 0) {
            printf("WARN %s could not pin to one CPU, skipped\n", name);
            return;
        }
    }
    g_t0 = now_us();
    g_deadline = g_t0 + window_ms * 1000.0;

    for (i = 0; i < nworkers; i++) {
        g_w[i].id = i + 1;
        g_w[i].t_create_call = now_us();
        if (pthread_create(&g_w[i].tid, NULL, tl_worker_fn, &g_w[i]) != 0) {
            printf("WARN pthread_create failed\n");
            nworkers = i;
            break;
        }
        g_w[i].t_create_ret = now_us();
    }

    printf("TLHEAD %s workers=%d window_ms=%.0f pinned_cpu=%d main_cpu=%d\n",
           name, nworkers, window_ms, pinned_cpu, sched_getcpu());

    for (i = 0; i < nworkers; i++) {
        tj0 = now_us();
        pthread_join(g_w[i].tid, NULL);
        tj1 = now_us();
        printf("TL %s main create %d %.1f %.1f\n", name, g_w[i].id,
               rel(g_w[i].t_create_call), rel(g_w[i].t_create_ret));
        printf("TL %s main join %d %.1f %.1f\n", name, g_w[i].id, rel(tj0), rel(tj1));
    }
    for (i = 0; i < nworkers; i++) {
        printf("TL %s t%d start %.1f\n", name, g_w[i].id, rel(g_w[i].t_start));
        for (k = 0; k < g_w[i].nseg; k++)
            printf("TL %s t%d run %.1f %.1f %d\n", name, g_w[i].id,
                   rel(g_w[i].segs[k].a), rel(g_w[i].segs[k].b), g_w[i].segs[k].cpu);
        printf("TL %s t%d end %.1f\n", name, g_w[i].id, rel(g_w[i].t_end));
    }
    if (pin)
        unpin();
}

/* =====================================================================
 * 2. RACE
 * Two threads each add 1 to a shared counter N times. Expected: 2*N.
 * ===================================================================== */

static volatile unsigned long g_count;     /* racy counter */
static unsigned long g_safe;               /* mutex-protected counter */
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_gate;                /* start both threads together */
static unsigned long g_n;

static void wait_gate(void)
{
    unsigned long spins = 0;
    while (!g_gate)
        spin_pause(&spins);
}

static void *race_plain_fn(void *arg)
{
    unsigned long i;
    (void)arg;
    wait_gate();
    for (i = 0; i < g_n; i++)
        g_count++;          /* load, add, store: NOT atomic, NOT protected */
    return NULL;
}

static void *race_mutex_fn(void *arg)
{
    unsigned long i;
    (void)arg;
    wait_gate();
    for (i = 0; i < g_n; i++) {
        pthread_mutex_lock(&g_mx);
        g_safe++;
        pthread_mutex_unlock(&g_mx);
    }
    return NULL;
}

static void race(const char *variant, void *(*fn)(void *), unsigned long n,
                 int runs, int pin)
{
    pthread_t a, b;
    int r;
    double t0, t1;
    unsigned long result;

    if (pin && pin_to_one_cpu() < 0) {
        printf("WARN %s could not pin, skipped\n", variant);
        return;
    }
    for (r = 1; r <= runs; r++) {
        g_count = 0;
        g_safe = 0;
        g_gate = 0;
        g_n = n;
        pthread_create(&a, NULL, fn, NULL);
        pthread_create(&b, NULL, fn, NULL);
        t0 = now_us();
        g_gate = 1;
        pthread_join(a, NULL);
        pthread_join(b, NULL);
        t1 = now_us();
        result = (fn == race_mutex_fn) ? g_safe : g_count;
        printf("RACE %s %d %lu %lu %lu %.0f\n", variant, r, n, result, 2 * n, t1 - t0);
        fflush(stdout);
    }
    if (pin)
        unpin();
}

/* =====================================================================
 * 3. SB - store buffering
 *   thread A:  X = 1;  r1 = Y;
 *   thread B:  Y = 1;  r2 = X;
 * In ANY one-step-at-a-time interleaving at least one store comes first,
 * so (r1, r2) = (0, 0) is impossible. Real x86 cores park stores in a
 * per-core store buffer, so the later load can complete first -> (0, 0).
 * ===================================================================== */

static volatile int X, Y, R1, R2;
static volatile unsigned long g_go[2], g_done[2];
static unsigned long g_iters;
static int g_fence;

static void *sb_fn(void *arg)
{
    int id = *(int *)arg;
    unsigned long it, spins = 0, seed = 12345UL + (unsigned long)id * 7919UL;
    volatile unsigned long d;
    unsigned long delay;

    for (it = 1; it <= g_iters; it++) {
        while (g_go[id] != it)
            spin_pause(&spins);
        FULL_BARRIER();
        seed = seed * 1103515245UL + 12345UL;   /* tiny random delay so   */
        delay = (seed >> 16) & 7UL;              /* the two threads overlap */
        for (d = 0; d < delay; d++)
            ;
        if (id == 0) {
            X = 1;
            if (g_fence) FULL_BARRIER(); else COMPILER_BARRIER();
            R1 = Y;
        } else {
            Y = 1;
            if (g_fence) FULL_BARRIER(); else COMPILER_BARRIER();
            R2 = X;
        }
        FULL_BARRIER();
        g_done[id] = it;
    }
    return NULL;
}

static void sb(const char *variant, unsigned long iters, int fence, int pin)
{
    pthread_t a, b;
    static int ida = 0, idb = 1;
    unsigned long it, spins = 0, c[4];
    double t0, t1;

    if (pin && pin_to_one_cpu() < 0) {
        printf("WARN %s could not pin, skipped\n", variant);
        return;
    }
    c[0] = c[1] = c[2] = c[3] = 0;
    g_iters = iters;
    g_fence = fence;
    g_go[0] = g_go[1] = g_done[0] = g_done[1] = 0;
    pthread_create(&a, NULL, sb_fn, &ida);
    pthread_create(&b, NULL, sb_fn, &idb);
    t0 = now_us();
    for (it = 1; it <= iters; it++) {
        X = 0;
        Y = 0;
        FULL_BARRIER();
        g_go[0] = it;
        g_go[1] = it;
        while (g_done[0] != it || g_done[1] != it)
            spin_pause(&spins);
        FULL_BARRIER();
        c[R1 * 2 + R2]++;
    }
    t1 = now_us();
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    /* counts of (r1,r2) = (0,0) (0,1) (1,0) (1,1) */
    printf("SB %s %lu %lu %lu %lu %lu %.0f\n", variant, iters,
           c[0], c[1], c[2], c[3], t1 - t0);
    fflush(stdout);
    if (pin)
        unpin();
}

/* ===================================================================== */

int main(int argc, char **argv)
{
    unsigned long race_n = 2000000UL, sb_n = 1000000UL;
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);

    if (argc > 1) race_n = strtoul(argv[1], NULL, 10);
    if (argc > 2) sb_n = strtoul(argv[2], NULL, 10);

#ifdef __CYGWIN__
    printf("INFO platform=cygwin");
#else
    printf("INFO platform=other");
#endif
    printf(" cpus=%ld gcc=%d.%d race_n=%lu sb_n=%lu\n",
           ncpu, __GNUC__, __GNUC_MINOR__, race_n, sb_n);
    fflush(stdout);

    timeline("free", 4, 20.0, 0);
    timeline("pin1", 4, 200.0, 1);

    race("plain-free", race_plain_fn, race_n, 5, 0);
    race("mutex-free", race_mutex_fn, race_n / 4, 3, 0);
    race("plain-pin1", race_plain_fn, race_n * 10, 5, 1);

    sb("plain-free", sb_n, 0, 0);
    sb("fence-free", sb_n, 1, 0);
    sb("plain-pin1", sb_n / 20, 0, 1);

    printf("DONE\n");
    return 0;
}
