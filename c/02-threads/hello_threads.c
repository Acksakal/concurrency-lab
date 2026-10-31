#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *worker(void *arg)
{
    int id = *(int *)arg;

    printf("hello from thread %d\n", id);
    return NULL;
}

int main(void)
{
    pthread_t tid[3];
    int ids[3];
    int i, rc;

    for (i = 0; i < 3; i++) {
        ids[i] = i;                       /* one slot per thread */
        rc = pthread_create(&tid[i], NULL, worker, &ids[i]);
        if (rc != 0) {                    /* error NUMBER, not errno */
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            return 1;
        }
    }
    for (i = 0; i < 3; i++)
        pthread_join(tid[i], NULL);       /* wait for each thread */
    printf("all threads finished\n");
    return 0;
}
