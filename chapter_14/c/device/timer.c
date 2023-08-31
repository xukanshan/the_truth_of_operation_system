#include "timer.h" 
#include "io.h"
#include "print.h"

#include "interrupt.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY	    100    //定义我们想要的中断发生频率，100HZ                         
#define INPUT_FREQUENCY	    1193180     //计数器0的工作脉冲信号评率
#define COUNTER0_VALUE	    INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	    0x40        //要写入初值的计数器端口号
#define COUNTER0_NO	        0   //要操作的计数器的号码
#define COUNTER_MODE	    2   //用在控制字中设定工作模式的号码，这里表示比率发生器
#define READ_WRITE_LATCH    3   //用在控制字中设定读/写/锁存操作位，这里表示先写入低字节，然后写入高字节
#define PIT_CONTROL_PORT    0x43    //控制字寄存器的端口

#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)

uint32_t ticks;          // ticks是内核自中断开启以来总共的嘀嗒数

/* 把操作的计数器counter_no、读写锁属性rwl、计数器模式counter_mode写入模式控制寄存器并赋予初始值counter_value */
static void frequency_set(uint8_t counter_port, \
			  uint8_t counter_no, \
			  uint8_t rwl, \
			  uint8_t counter_mode, \
			  uint16_t counter_value) {
/* 往控制字寄存器端口0x43中写入控制字 */
   outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
/* 先写入counter_value的低8位 */
   outb(counter_port, (uint8_t)counter_value);
/* 再写入counter_value的高8位 */
   //outb(counter_port, (uint8_t)counter_value >> 8); 作者这句代码会先将16位的counter_value强制类型转换为8位值，也就是原来16位值只留下了低8位，然后
   //又右移8未，所以最后送入counter_port的counter_value的高8位是8个0，这会导致时钟频率过高，出现GP异常
   outb(counter_port, (uint8_t) (counter_value>>8) );
}

/* 时钟的中断处理函数 */
static void intr_timer_handler(void) {
   struct task_struct* cur_thread = running_thread();

   ASSERT(cur_thread->stack_magic == 0x19870916);         // 检查栈是否溢出

   cur_thread->elapsed_ticks++;	  // 记录此线程占用的cpu时间嘀
   ticks++;	  //从内核第一次处理时间中断后开始至今的滴哒数,内核态和用户态总共的嘀哒数

   if (cur_thread->ticks == 0) {	  // 若进程时间片用完就开始调度新的进程上cpu
      schedule(); 
   } 
   else {				  // 将当前进程的时间片-1
      cur_thread->ticks--;
   }
}



/* 初始化PIT8253 */
void timer_init() {
   put_str("timer_init start\n");
   /* 设置8253的定时周期,也就是发中断的周期 */
   frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
   register_handler(0x20, intr_timer_handler);
   put_str("timer_init done\n");
}

/* 以tick为单位的sleep,任何时间形式的sleep会转换此ticks形式 */
static void ticks_to_sleep(uint32_t sleep_ticks) {
   uint32_t start_tick = ticks;
   /* 若间隔的ticks数不够便让出cpu */
   while (ticks - start_tick < sleep_ticks) {
      thread_yield();
   }
}

/* 以毫秒为单位的sleep   1秒= 1000毫秒 */
void mtime_sleep(uint32_t m_seconds) {
   uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
   ASSERT(sleep_ticks > 0);
   ticks_to_sleep(sleep_ticks); 
}
