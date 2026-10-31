#include <pthread.h>
#include <stdio.h>

#define N 1000000L

static long counter = 0;                  /* shared by both threads */

static void *work(void *arg)
{
    long i;

    (void)arg;
    for (i = 0; i < N; i++)
        counter++;                        /* not atomic: load, add, store */
    return NULL;
}

int main(void)
{
    pthread_t a, b;

    pthread_create(&a, NULL, work, NULL);
    pthread_create(&b, NULL, work, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("expected %ld, got %ld\n", 2 * N, counter);
    return 0;
}
