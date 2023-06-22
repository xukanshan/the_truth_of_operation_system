#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"

#define KBD_BUF_PORT 0x60	   // 键盘buffer寄存器端口号为0x60

/* 键盘中断处理程序 */
static void intr_keyboard_handler(void) {
   put_char('k');
//每次必须要从8042读走键盘8048传递过来的数据，否则8042不会接收后续8048传递过来的数据
   inb(KBD_BUF_PORT);
   return;
}

/* 键盘初始化 */
void keyboard_init() {
   put_str("keyboard init start\n");
   register_handler(0x21, intr_keyboard_handler);       //注册键盘中断处理函数
   put_str("keyboard init done\n");
}
