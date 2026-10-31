#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        printf("child  %d (parent %d)\n", getpid(), getppid());
    } else {
        printf("parent %d made child %d\n", getpid(), pid);
        wait(NULL);
    }
    return 0;
}
