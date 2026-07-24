#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

void *count_words(void *);
int total_words = 0;

int main(int argc, char *argv[])
{
    pthread_t t1, t2;

    if (argc != 3)
    {
        printf("usage: %s file1 file2\n", argv[0]);
        exit(1);
    }

    pthread_create(&t1, NULL, count_words, argv[1]);
    pthread_create(&t2, NULL, count_words, argv[2]);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("%5d: total characters\n", total_words);
}

void *count_words(void *filep)
{
    char *file = (char *)filep;
    FILE *fp = fopen(file, "r");
    if (fp == NULL)
    {
        perror(file);
        return NULL;
    }

    while (getc(fp) != EOF)
        total_words++;

    fclose(fp);
    return NULL;
}
