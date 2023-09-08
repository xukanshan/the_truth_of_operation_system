#include "print.h"
#include "init.h"
#include "fs.h"
#include "stdio.h"
#include "string.h"

int main(void) {
   put_str("I am kernel\n");
   init_all();
   printf("/file1 delete %s!\n", sys_unlink("/file1") == 0 ? "done" : "fail");
   while(1);
   return 0;
}