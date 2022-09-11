#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int prime_select(int in, int prime) //select prime to next pipe and return to be the new stream
{
    int n;
    int out[2];

    pipe(out);

    if (fork() == 0) {  //open a new process to select prime to next pipe
        while (read(in, &n, sizeof(int))) {
            if (n % prime != 0) {
                write(out[1], &n, sizeof(int));
            }
        }

        close(in);
        close(out[1]);
        exit(0);
    }
    else {
        close(in);
        close(out[1]);
    }

    return out[0];  //return new pipe to change input stream
}

int main(int argc, char* argv[])
{
    int p[2];   //pipe
    int i;
    int prime;  //prime to display

    pipe(p);
    //init the first input stream
    for (i = 2; i <= 35; i++) {
        write(p[1], &i, sizeof(int));
    }
    close(p[1]);
    //display prime and change input stream
    while(read(p[0], &prime, sizeof(int))) {
        printf("prime %d\n", prime);
        p[0] = prime_select(p[0], prime);
    }
    close(p[0]);

    exit(0);
}
