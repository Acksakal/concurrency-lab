#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t can_put  = PTHREAD_COND_INITIALIZER;
static pthread_cond_t can_take = PTHREAD_COND_INITIALIZER;
static int full = 0;                      /* the predicate: is the box full? */
static int box = 0;                       /* holds at most one item */

static void *producer(void *arg)
{
    int i;

    (void)arg;
    for (i = 1; i <= 5; i++) {
        pthread_mutex_lock(&m);
        while (full)                      /* re-check after every wakeup */
            pthread_cond_wait(&can_put, &m);
        box = i;
        full = 1;
        printf("put  %d\n", i);
        pthread_cond_signal(&can_take);
        pthread_mutex_unlock(&m);
    }
    return NULL;
}

static void *consumer(void *arg)
{
    int i;

    (void)arg;
    for (i = 1; i <= 5; i++) {
        pthread_mutex_lock(&m);
        while (!full)
            pthread_cond_wait(&can_take, &m);
        printf("took %d\n", box);
        full = 0;
        pthread_cond_signal(&can_put);
        pthread_mutex_unlock(&m);
    }
    return NULL;
}

int main(void)
{
    pthread_t p, c;

    pthread_create(&c, NULL, consumer, NULL);
    pthread_create(&p, NULL, producer, NULL);
    pthread_join(p, NULL);
    pthread_join(c, NULL);
    return 0;
}
