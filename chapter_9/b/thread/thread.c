#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"

#define PG_SIZE 4096

/* 由kernel_thread去执行function(func_arg) , 这个函数就是线程中去开启我们要运行的函数*/
static void kernel_thread(thread_func* function, void* func_arg) {
   function(func_arg); 
}

/*用于根据传入的线程的pcb地址、要运行的函数地址、函数的参数地址来初始化线程栈中的运行信息，核心就是填入要运行的函数地址与参数 */
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
   /* 先预留中断使用栈的空间,可见thread.h中定义的结构 */
   //pthread->self_kstack -= sizeof(struct intr_stack);  //-=结果是sizeof(struct intr_stack)的4倍
   //self_kstack类型为uint32_t*，也就是一个明确指向uint32_t类型值的地址，那么加减操作，都是会是sizeof(uint32_t) = 4 的倍数
   pthread->self_kstack = (uint32_t*)((int)(pthread->self_kstack) - sizeof(struct intr_stack));

   /* 再留出线程栈空间,可见thread.h中定义 */
   //pthread->self_kstack -= sizeof(struct thread_stack);
   pthread->self_kstack = (uint32_t*)((int)(pthread->self_kstack) - sizeof(struct thread_stack));
   struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;     //我们已经留出了线程栈的空间，现在将栈顶变成一个线程栈结构体
                                                                                         //指针，方便我们提前布置数据达到我们想要的目的
   kthread_stack->eip = kernel_thread;      //我们将线程的栈顶指向这里，并ret，就能直接跳入线程启动器开始执行。
                                            //为什么这里我不能直接填传入进来的func，这也是函数地址啊，为什么还非要经过一个启动器呢？其实是可以不经过线程启动器的

    //因为用不着，所以不用初始化这个返回地址kthread_stack->unused_retaddr
   kthread_stack->function = function;      //将线程启动器（thread_start）需要运行的函数地址放入线程栈中
   kthread_stack->func_arg = func_arg;      //将线程启动器（thread_start）需要运行的函数所需要的参数地址放入线程栈中
   kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

/* 初始化线程基本信息 , pcb中存储的是线程的管理信息，此函数用于根据传入的pcb的地址，线程的名字等来初始化线程的管理信息*/
void init_thread(struct task_struct* pthread, char* name, int prio) {
   memset(pthread, 0, sizeof(*pthread));                                //把pcb初始化为0
   strcpy(pthread->name, name);                                         //将传入的线程的名字填入线程的pcb中
   pthread->status = TASK_RUNNING;                                      //这个函数是创建线程的一部分，自然线程的状态就是运行态
   pthread->priority = prio;            
                                                                        /* self_kstack是线程自己在内核态下使用的栈顶地址 */
   pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);     //本操作系统比较简单，线程不会太大，就将线程栈顶定义为pcb地址
                                                                        //+4096的地方，这样就留了一页给线程的信息（包含管理信息与运行信息）空间
   pthread->stack_magic = 0x19870916;	                                // /定义的边界数字，随便选的数字来判断线程的栈是否已经生长到覆盖pcb信息了              
}

/* 创建一优先级为prio的线程,线程名为name,线程所执行的函数是function(func_arg) */
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
/* pcb都位于内核空间,包括用户进程的pcb也是在内核空间 */
   struct task_struct* thread = get_kernel_pages(1);    //为线程的pcb申请4K空间的起始地址

   init_thread(thread, name, prio);                     //初始化线程的pcb
   thread_create(thread, function, func_arg);           //初始化线程的线程栈

            //我们task_struct->self_kstack指向thread_stack的起始位置，然后pop升栈，
            //到了通过线程启动器来的地址，ret进入去运行真正的实际函数
            //通过ret指令进入，原因：1、函数地址与参数可以放入栈中统一管理；2、ret指令可以直接从栈顶取地址跳入执行
   asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret" : : "g" (thread->self_kstack) : "memory");
   return thread;
}
