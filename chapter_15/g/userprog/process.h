#ifndef __USERPROG_PROCESS_H 
#define __USERPROG_PROCESS_H 
#include "thread.h"
#define default_prio 31 //定义默认的优先级
#define USER_STACK3_VADDR  (0xc0000000 - 0x1000)    //定义了一页C语言程序的栈顶起始地址（虚拟）,书p511
#define USER_VADDR_START 0x8048000	 //linux下大部分可执行程序的入口地址（虚拟）都是这个附近，我们也仿照这个设定
void create_user_vaddr_bitmap(struct task_struct* user_prog);
uint32_t* create_page_dir(void);
void start_process(void* filename_);
void intr_init(void* func);
void page_dir_activate(struct task_struct* p_thread);
void process_execute(void* filename, char* name);
void process_activate(struct task_struct* p_thread);
#endif


