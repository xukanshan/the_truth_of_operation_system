#include "print.h"
#include "init.h"
#include "fs.h"
#include "stdio.h"

int main(void) {
   put_str("I am kernel\n");
   init_all();
/********  测试代码  ********/
   char cwd_buf[32] = {0};
   sys_getcwd(cwd_buf, 32);
   printf("cwd:%s\n", cwd_buf);
   sys_chdir("/dir1");
   printf("change cwd now\n");
   sys_getcwd(cwd_buf, 32);
   printf("cwd:%s\n", cwd_buf);
/********  测试代码  ********/
   while(1);
   return 0;
}