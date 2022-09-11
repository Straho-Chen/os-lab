#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fs.h"

void find(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // normal judge
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)
    {
        printf("find: path too long\n");
        return;
    }

    // open the file in the path
    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }
    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    // for all files in the path, judge its name whether is filename
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        // add suffix
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        if (de.inum == 0)
            continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;

        // get the file info in the path
        if (stat(buf, &st) < 0)
        {
            printf("find: cannot stat %s\n", buf);
            continue;
        }

        // for different type do different thing
        switch (st.type)
        {
        case T_FILE: // if find the file, print it
            if (strcmp(filename, de.name) == 0)
            {
                printf("%s\n", buf);
            }
            break;

        case T_DIR: // if find a dir, if it isn't "." or "..", then find again
            if (strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0)
            {
                find(buf, filename);
            }
            break;
        }
    }

    close(fd);
    return;
}

int main(int argc, char *argv[])
{
    int i;
    // judge argc number
    if (argc < 2)
    {
        printf("find: requires a file name to search");
    }
    else if (argc == 2)
    {
        find(".", argv[1]);
    }
    else
    {
        // muti find
        for (i = 2; i < argc; i++)
        {
            find(argv[1], argv[i]);
        }
    }

    exit(0);
}
