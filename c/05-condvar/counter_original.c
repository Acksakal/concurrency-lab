#include <pthread.h>
#include <stdio.h>

#define COUNT_DONE 10
#define COUNT_HALT1 3
#define COUNT_HALT2 6

pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;

int count = 0;

void *functionCount1(void *arg)
{
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&count_mutex);

        while (count >= COUNT_HALT1 &&
               count <= COUNT_HALT2) {
            pthread_cond_wait(&condition_cond, &count_mutex);
        }

        if (count >= COUNT_DONE) {
            pthread_mutex_unlock(&count_mutex);
            return NULL;
        }

        count++;
        printf("Counter value functionCount1: %d\n", count);

        pthread_mutex_unlock(&count_mutex);
    }
}

void *functionCount2(void *arg)
{
    (void)arg;

    for (;;) {
        pthread_mutex_lock(&count_mutex);

        if (count >= COUNT_DONE) {
            pthread_cond_broadcast(&condition_cond);
            pthread_mutex_unlock(&count_mutex);
            return NULL;
        }

        count++;
        printf("Counter value functionCount2: %d\n", count);

        pthread_mutex_unlock(&count_mutex);
    }
}

int main(void)
{
    pthread_t t1, t2;

    if (pthread_create(&t1, NULL, functionCount1, NULL) != 0)
        return 1;

    if (pthread_create(&t2, NULL, functionCount2, NULL) != 0) {
        pthread_join(t1, NULL);
        return 1;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
