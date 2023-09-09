#include "print.h"
#include "init.h"
#include "fs.h"
#include "stdio.h"

int main(void)
{
    put_str("I am kernel\n");
    init_all();
    /********  测试代码  ********/
    struct stat obj_stat;
    sys_stat("/", &obj_stat);
    printf("/`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n",
           obj_stat.st_ino, obj_stat.st_size,
           obj_stat.st_filetype == 2 ? "directory" : "regular");
    sys_stat("/dir1", &obj_stat);
    printf("/dir1`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n",
           obj_stat.st_ino, obj_stat.st_size,
           obj_stat.st_filetype == 2 ? "directory" : "regular");
    /********  测试代码  ********/
    while (1)
        ;
    return 0;
}