#include <pthread.h>
#include <stdio.h>
#include <time.h>

static pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;

static void pause_a_moment(void)          /* widen the window so it always happens */
{
    struct timespec t = { 0, 100000000L };    /* 100 ms */

    nanosleep(&t, NULL);
}

static void *thread_a(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m1);
    pause_a_moment();
    pthread_mutex_lock(&m2);              /* waits for B, who waits for A */
    puts("A got both locks");
    pthread_mutex_unlock(&m2);
    pthread_mutex_unlock(&m1);
    return NULL;
}

static void *thread_b(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&m2);              /* opposite order: the bug */
    pause_a_moment();
    pthread_mutex_lock(&m1);
    puts("B got both locks");
    pthread_mutex_unlock(&m1);
    pthread_mutex_unlock(&m2);
    return NULL;
}

int main(void)
{
    pthread_t a, b;

    pthread_create(&a, NULL, thread_a, NULL);
    pthread_create(&b, NULL, thread_b, NULL);
    pthread_join(a, NULL);                /* never returns */
    pthread_join(b, NULL);
    return 0;
}
