#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("xargs: requires more arguments\n");
        exit(-1);
    }

    // move argv[] to buff, and remove the first shell command
    char *buff[MAXARG];
    int i;
    int cnt = 0;
    for (i = 1; i < argc; i++)
    {
        buff[i - 1] = argv[i];
        cnt++;
    }

    // read stdin and add to buff in child process
    char *in = malloc(512 * sizeof(char));
    while (gets(in, 512))
    {
        int len = strlen(in);
        if (len == 0)
        {
            break;
        }
        else
        {
            in[len - 1] = 0;
            buff[cnt++] = in;
            buff[cnt++] = 0;
            if (!fork())
            {
                // exec command
                exec(argv[1], buff);
            }
            wait(0);
        }
    }

    free(buff);
    free(in);

    exit(0);
}