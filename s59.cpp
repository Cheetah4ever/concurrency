#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

void *print(void *arg)
{
    const char *mesg = static_cast<const char *>(arg);

    for (int i = 0; i < 5; i++)
    {
        printf("%s\n", mesg);
        sleep(1);
    }

    return NULL;
}

int main()
{
    pthread_t t1, t2;

    pthread_create(&t1, NULL, print, (void *)"hello");
    pthread_create(&t2, NULL, print, (void *)"world");

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
