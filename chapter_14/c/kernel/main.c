#include "print.h"
#include "init.h"
#include "fs.h"


int main(void) {
   put_str("I am kernel\n");
   init_all();
   sys_open("/file1", O_CREAT);
   while(1);
   return 0;
}
