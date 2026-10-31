/*
 * single_count.c - the "blue bar" work from ryzen_lab, done by ONE thread.
 *
 * ryzen_lab's blue bar: 2 threads x 500,000 increments, each one wrapped in
 * pthread_mutex_lock/unlock. Total: 1,000,000 increments.
 *
 * Here main() alone does the same 1,000,000 increments, two ways:
 *   A. plain:       count++                      (no lock at all)
 *   B. with mutex:  lock; count++; unlock;       (same calls as the blue bar,
 *                                                 but nobody else wants the lock)
 * No thread is ever created. B only uses the mutex functions, so it still
 * needs -pthread to link.
 *
 * Build: gcc -std=gnu90 -O2 -Wall -Wextra -pedantic -pthread single_count.c -o single
 * Run:   ./single            (default N = 500000 per "thread", total 2N)
 *        ./single 5000000
 */
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static volatile unsigned long g_count;   /* volatile: same loads/stores as in ryzen_lab */
static pthread_mutex_t g_mx = PTHREAD_MUTEX_INITIALIZER;

/* Wall-clock time in milliseconds (clock_gettime is declared in time.h). */
static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static void run_plain(unsigned long total)
{
    unsigned long i;
    for (i = 0; i < total; i++)
        g_count++;
}

static void run_mutex(unsigned long total)
{
    unsigned long i;
    for (i = 0; i < total; i++) {
        pthread_mutex_lock(&g_mx);
        g_count++;
        pthread_mutex_unlock(&g_mx);
    }
}

static void measure(const char *name, void (*fn)(unsigned long), unsigned long total)
{
    int r;
    double t0, t1;

    for (r = 1; r <= 3; r++) {
        g_count = 0;
        t0 = now_ms();
        fn(total);
        t1 = now_ms();
        printf("%-12s run %d: count = %lu (expected %lu)  time = %10.3f ms  (%.1f ns per increment)\n",
               name, r, (unsigned long)g_count, total, t1 - t0,
               (t1 - t0) * 1e6 / (double)total);
    }
}

int main(int argc, char **argv)
{
    unsigned long n = 500000UL;              /* per "thread", like ryzen_lab's blue bar */

    if (argc > 1)
        n = strtoul(argv[1], NULL, 10);

    printf("One thread doing 2 x %lu = %lu increments\n\n", n, 2 * n);
    measure("plain", run_plain, 2 * n);
    printf("\n");
    measure("with mutex", run_mutex, 2 * n);
    printf("\nCompare with the blue bar in ryzen_lab (2 threads, same %lu increments).\n", 2 * n);
    return 0;
}
