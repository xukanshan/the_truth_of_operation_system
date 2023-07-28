#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"
#include "thread.h"

/* 信号量结构 */
struct semaphore {
   uint8_t value;              //一个信号量肯定有值来表示这个量
   struct list waiters;       //用一个双链表结点来管理所有阻塞在该信号量上的线程
};

/* 锁结构 */
struct lock {
   struct   task_struct* holder;	    //用于记录谁把二元信号量申请走了，而导致了该信号量的锁
   struct   semaphore semaphore;	    //一个锁肯定是来管理信号量的
   uint32_t holder_repeat_nr;		    //有时候线程拿到了信号量，但是线程内部不止一次使用该信号量对应公共资源，就会不止一次申请锁
                                        //内外层函数在释放锁时就会对一个锁释放多次，所以必须要记录重复申请的次数
};

void sema_init(struct semaphore* psema, uint8_t value); 
void sema_down(struct semaphore* psema);
void sema_up(struct semaphore* psema);
void lock_init(struct lock* plock);
void lock_acquire(struct lock* plock);
void lock_release(struct lock* plock);
#endif
