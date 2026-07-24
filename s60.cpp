#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

void print(const char *mesg)
{
    for (int i = 0; i < 5; i++)
    {
        printf("%s\n", mesg);
        fflush(stdout);
        sleep(1);
    }
}

int main()
{
    pid_t p1 = fork();

    if (p1 == 0)
    {
        print("hello");
        _exit(0);
    }

    pid_t p2 = fork();

    if (p2 == 0)
    {
        print("world");
        _exit(0);
    }

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);

    return 0;
}
