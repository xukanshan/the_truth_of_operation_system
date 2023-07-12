#include "process.h"
#include "thread.h"
#include "global.h"   //定义了PG_SIZE
#include "memory.h"
#include "bitmap.h"
#include "string.h"
#include "tss.h"
#include "console.h"
#include "debug.h"
#include "interrupt.h"

//用于初始化进程pcb中的用于管理自己虚拟地址空间的虚拟内存池结构体
void create_user_vaddr_bitmap(struct task_struct* user_prog) {
   user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
   uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8 , PG_SIZE);      //计算出管理用于进程那么大的虚拟地址的
                                                                                                        //位图需要多少页的空间来存储（向上取整结果）
   user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);                       //申请位图空间
   user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;   //计算出位图长度（字节单位）
   bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);        //初始化位图
}


//用于为进程创建页目录表，并初始化（系统映射+页目录表最后一项是自己的物理地址，以此来动态操作页目录表），成功后，返回页目录表虚拟地址，失败返回空地址
uint32_t* create_page_dir(void) {
   uint32_t* page_dir_vaddr = get_kernel_pages(1);  //用户进程的页表不能让用户直接访问到,所以在内核空间来申请
   if (page_dir_vaddr == NULL) {
        console_put_str("create_page_dir: get_kernel_page failed!");
        return NULL;
   }
   //将内核页目录表的768号项到1022号项复制过来
   memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 768*4), (uint32_t*)(0xfffff000 + 768 * 4), 255 * 4);
   uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);     //将进程的页目录表的虚拟地址，转换成物理地址
   page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;   //页目录表最后一项填自己的地址，为的是动态操作页表
   return page_dir_vaddr;
}

extern void intr_exit(void);

//用于初始化进入进程所需要的中断栈中的信息，传入参数是实际要运行的函数地址(进程)，这个函数是用线程启动器进入的（kernel_thread）
void start_process(void* filename_) {
   void* function = filename_;
   struct task_struct* cur = running_thread();
   cur->self_kstack += sizeof(struct thread_stack); //当我们进入到这里的时候，cur->self_kstack指向thread_stack的起始地址，跳过这里，才能设置intr_stack
   struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;	 
   proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
   proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
   proc_stack->gs = 0;		 //用户态根本用不上这个，所以置为0（gs我们一般用于访问显存段，这个让内核态来访问）
   proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;      
   proc_stack->eip = function;	 //设定要执行的函数（进程）的地址
   proc_stack->cs = SELECTOR_U_CODE;
   proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);     //设置用户态下的eflages的相关字段
    //下面这一句是在初始化中断栈中的栈顶位置，我们先为虚拟地址0xc0000000 - 0x1000申请了个物理页，然后将虚拟地址+4096置为栈顶
   proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE) ;
   proc_stack->ss = SELECTOR_U_DATA; 
   asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}

/* 激活页表 */
void page_dir_activate(struct task_struct* p_thread) {
/********************************************************
 * 执行此函数时,当前任务可能是线程。
 * 之所以对线程也要重新安装页表, 原因是上一次被调度的可能是进程,
 * 否则不恢复页表的话,线程就会使用进程的页表了。
 ********************************************************/

/* 若为内核线程,需要重新填充页表为0x100000 */
   uint32_t pagedir_phy_addr = 0x100000;  // 默认为内核的页目录物理地址,也就是内核线程所用的页目录表
   if (p_thread->pgdir != NULL)	{    //如果不为空，说明要调度的是个进程，那么就要执行加载页表，所以先得到进程页目录表的物理地址
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
   }
   asm volatile ("movl %0, %%cr3" : : "r" (pagedir_phy_addr) : "memory");   //更新页目录寄存器cr3,使新页表生效
}


//用于加载进程自己的页目录表，同时更新进程自己的0特权级esp0到TSS中
void process_activate(struct task_struct* p_thread) {
    ASSERT(p_thread != NULL);
   /* 激活该进程或线程的页表 */
    page_dir_activate(p_thread);
   /* 内核线程特权级本身就是0,处理器进入中断时并不会从tss中获取0特权级栈地址,故不需要更新esp0 */
    if (p_thread->pgdir)
        update_tss_esp(p_thread);   /* 更新该进程的esp0,用于此进程被中断时保留上下文 */
}

//用于创建进程，参数是进程要执行的函数与他的名字
void process_execute(void* filename, char* name) { 
    /* pcb内核的数据结构,由内核来维护进程信息,因此要在内核内存池中申请 */
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, default_prio); 
    create_user_vaddr_bitmap(thread);
    thread_create(thread, start_process, filename);
    thread->pgdir = create_page_dir();
    
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    intr_set_status(old_status);
}