#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int main(int argc, char* argv[])
{
    int child_p[2];   //read p[0], write p[1]
    int parent_p[2];
    pipe(child_p); /*正常创建后，p[1]为管道写入端，p[0]为管道读出端*/ 
    pipe(parent_p);

    /* 子进程读管道，父进程写管道 */
    int pid = fork();
    if (pid == 0) { 
        /* 子进程 */
        close(parent_p[1]); // 关闭写端
        char child_buff[6];
        read(parent_p[0], child_buff, 5);
        pid = getpid();
        printf("%d: received %s\n", pid, child_buff);
        close(parent_p[0]); // 读取完成，关闭读端
        
        close(child_p[0]);
        write(child_p[1], "pong", 5);
        close(child_p[1]);
    } else if (pid > 0) { 
        /* 父进程 */
        close(parent_p[0]); // 关闭读端
        write(parent_p[1], "ping", 5);
        close(parent_p[1]); // 写入完成，关闭写端

        close(child_p[1]); // 关闭写端
        char parent_buff[6];
        read(child_p[0], parent_buff, 5);
        pid = getpid();
        printf("%d: received %s\n", pid, parent_buff);
        close(child_p[0]); // 关闭读端
    }
    
    exit(0);
}
