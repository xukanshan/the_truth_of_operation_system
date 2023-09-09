#include "print.h"
#include "init.h"
#include "fs.h"
#include "stdio.h"
#include "string.h"
#include "dir.h"

int main(void)
{
    put_str("I am kernel\n");
    init_all();
    /********  测试代码  ********/
    printf("/dir1 content before delete /dir1/subdir1:\n");
    struct dir *dir = sys_opendir("/dir1/");
    char *type = NULL;
    struct dir_entry *dir_e = NULL;
    while ((dir_e = sys_readdir(dir)))
    {
        if (dir_e->f_type == FT_REGULAR)
        {
            type = "regular";
        }
        else
        {
            type = "directory";
        }
        printf("      %s   %s\n", type, dir_e->filename);
    }
    printf("try to delete nonempty directory /dir1/subdir1\n");
    if (sys_rmdir("/dir1/subdir1") == -1)
    {
        printf("sys_rmdir: /dir1/subdir1 delete fail!\n");
    }

    printf("try to delete /dir1/subdir1/file2\n");
    if (sys_rmdir("/dir1/subdir1/file2") == -1)
    {
        printf("sys_rmdir: /dir1/subdir1/file2 delete fail!\n");
    }
    if (sys_unlink("/dir1/subdir1/file2") == 0)
    {
        printf("sys_unlink: /dir1/subdir1/file2 delete done\n");
    }

    printf("try to delete directory /dir1/subdir1 again\n");
    if (sys_rmdir("/dir1/subdir1") == 0)
    {
        printf("/dir1/subdir1 delete done!\n");
    }

    printf("/dir1 content after delete /dir1/subdir1:\n");
    sys_rewinddir(dir);
    while ((dir_e = sys_readdir(dir)))
    {
        if (dir_e->f_type == FT_REGULAR)
        {
            type = "regular";
        }
        else
        {
            type = "directory";
        }
        printf("      %s   %s\n", type, dir_e->filename);
    }

    /********  测试代码  ********/
    while (1)
        ;
    return 0;
}