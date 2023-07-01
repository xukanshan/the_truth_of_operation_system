#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"

//用于初始化信号量，传入参数就是指向信号量的指针与初值
void sema_init(struct semaphore* psema, uint8_t value) {
   psema->value = value;       // 为信号量赋初值
   list_init(&psema->waiters); //初始化信号量的等待队列
}

//用于初始化锁，传入参数是指向该锁的指针
void lock_init(struct lock* plock) {
   plock->holder = NULL;
   plock->holder_repeat_nr = 0;
   sema_init(&plock->semaphore, 1); //将信号量初始化为1，因为此函数一般处理二元信号量
}

//信号量的down操作，也就是减1操作，传入参数是指向要操作的信号量指针。线程想要申请信号量的时候用此函数
void sema_down(struct semaphore* psema) {
   enum intr_status old_status = intr_disable();         //对于信号量的操作是必须关中断的

   //一个自旋锁，来不断判断是否信号量已经被分配出去了。为什么不用if，见书p450。
    while(psema->value == 0) {	// 若value为0,表示已经被别人持有
        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        /* 当前线程不应该已在信号量的waiters队列中 */
        if (elem_find(&psema->waiters, &running_thread()->general_tag)) {
	        PANIC("sema_down: thread blocked has been in waiters_list\n");
        }
        //如果此时信号量为0，那么就将该线程加入阻塞队列,为什么不用判断是否在阻塞队列中呢？因为线程被阻塞后，会加入阻塞队列，除非被唤醒，否则不会
        //分配到处理器资源，自然也不会重复判断是否有信号量，也不会重复加入阻塞队列
        list_append(&psema->waiters, &running_thread()->general_tag); 
        thread_block(TASK_BLOCKED);    // 阻塞线程,直到被唤醒
    }
/* 若value为1或被唤醒后,会执行下面的代码,也就是获得了锁。*/
    psema->value--;
    ASSERT(psema->value == 0);	    
/* 恢复之前的中断状态 */
    intr_set_status(old_status);
}

//信号量的up操作，也就是+1操作，传入参数是指向要操作的信号量的指针。且释放信号量时，应唤醒阻塞在该信号量阻塞队列上的一个进程
void sema_up(struct semaphore* psema) {
/* 关中断,保证原子操作 */
   enum intr_status old_status = intr_disable();
   ASSERT(psema->value == 0);	    
   if (!list_empty(&psema->waiters)) {   //判断信号量阻塞队列应为非空，这样才能执行唤醒操作
      struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
      thread_unblock(thread_blocked);
   }
   psema->value++;
   ASSERT(psema->value == 1);	    
/* 恢复之前的中断状态 */
   intr_set_status(old_status);
}

//获取锁的函数,传入参数是指向锁的指针
void lock_acquire(struct lock* plock) {
//这是为了排除掉线程自己已经拿到了锁，但是还没有释放就重新申请的情况
   if (plock->holder != running_thread()) { 
        sema_down(&plock->semaphore);    //对信号量进行down操作
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;    //申请了一次锁
   } else {
        plock->holder_repeat_nr++;
   }
}

//释放锁的函数，参数是指向锁的指针
void lock_release(struct lock* plock) {
   ASSERT(plock->holder == running_thread());
   //如果>1，说明自己多次申请了该锁，现在还不能立即释放锁
   if (plock->holder_repeat_nr > 1) {   
      plock->holder_repeat_nr--;
      return;
   }
   ASSERT(plock->holder_repeat_nr == 1);    //判断现在lock的重复持有数是不是1只有为1，才能释放

   plock->holder = NULL;	   //这句必须放在up操作前，因为现在并不在关中断下运行，有可能会被切换出去，如果在up后面，就可能出现还没有置空，
                                //就切换出去，此时有了信号量，下个进程申请到了，将holder改成下个进程，这个进程切换回来就把holder改成空，就错了
   plock->holder_repeat_nr = 0;
   sema_up(&plock->semaphore);	   // 信号量的V操作,也是原子操作
}

