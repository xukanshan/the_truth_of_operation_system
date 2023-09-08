#include "print.h"
#include "init.h"
#include "fs.h"
#include "stdio.h"
#include "string.h"

int main(void)
{
    put_str("I am kernel\n");
    init_all();
    struct dir *p_dir = sys_opendir("/dir1/subdir1");
    if (p_dir)
    {
        printf("/dir1/subdir1 open done!\n");
        if (sys_closedir(p_dir) == 0)
        {
            printf("/dir1/subdir1 close done!\n");
        }
        else
        {
            printf("/dir1/subdir1 close fail!\n");
        }
    }
    else
    {
        printf("/dir1/subdir1 open fail!\n");
    }
    while (1)
        ;
    return 0;
}