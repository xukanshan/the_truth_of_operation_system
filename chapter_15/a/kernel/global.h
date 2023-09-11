#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"


// ----------------  GDT描述符属性  ----------------

#define	DESC_G_4K    1
#define	DESC_D_32    1
#define DESC_L	     0	        // 64位代码标记，此处标记为0便可。
#define DESC_AVL     0	        // cpu不用此位，暂置为0  
#define DESC_P	     1
#define DESC_DPL_0   0
#define DESC_DPL_1   1
#define DESC_DPL_2   2
#define DESC_DPL_3   3
#define DESC_S_CODE	1
#define DESC_S_DATA	DESC_S_CODE
#define DESC_S_SYS	0
#define DESC_TYPE_CODE	8	// x=1,c=0,r=0,a=0 代码段是可执行的,非依从的,不可读的,已访问位a清0.  
#define DESC_TYPE_DATA  2	// x=0,e=0,w=1,a=0 数据段是不可执行的,向上扩展的,可写的,已访问位a清0.
#define DESC_TYPE_TSS   9	// B位为0,不忙


//选择子的RPL字段
#define	 RPL0  0
#define	 RPL1  1
#define	 RPL2  2
#define	 RPL3  3

//选择子的TI字段
#define TI_GDT 0
#define TI_LDT 1

//定义不同的内核用的段描述符选择子
#define SELECTOR_K_CODE	   ((1 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_DATA	   ((2 << 3) + (TI_GDT << 2) + RPL0)
#define SELECTOR_K_STACK   SELECTOR_K_DATA 
#define SELECTOR_K_GS	   ((3 << 3) + (TI_GDT << 2) + RPL0)
//定义不同的用户程序用的段描述符选择子
#define SELECTOR_U_CODE	   ((5 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_DATA	   ((6 << 3) + (TI_GDT << 2) + RPL3)
#define SELECTOR_U_STACK   SELECTOR_U_DATA


#define GDT_ATTR_HIGH		 ((DESC_G_4K << 7) + (DESC_D_32 << 6) + (DESC_L << 5) + (DESC_AVL << 4))    //定义段描述符的高32位的高字
#define GDT_CODE_ATTR_LOW_DPL3	 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_CODE << 4) + DESC_TYPE_CODE)  //定义用户程序用的代码段描述符高32位的低字
#define GDT_DATA_ATTR_LOW_DPL3	 ((DESC_P << 7) + (DESC_DPL_3 << 5) + (DESC_S_DATA << 4) + DESC_TYPE_DATA)  //定义用户程序用的数据段描述符高32位的低字

//---------------  TSS描述符属性  ------------
#define TSS_DESC_D  0   //这个D/B位在其他段描述中用于表示操作数的大小，但这里不是，实际上它根本就没有被使用（总是设置为0）。
                        //这是因为TSS的大小和结构并不依赖于处理器运行在16位模式还是32位模式。
                        //无论何时，TSS都包含了32位的寄存器值、32位的线性地址等等，因此没有必要用D/B位来表示操作的大小

#define TSS_ATTR_HIGH ((DESC_G_4K << 7) + (TSS_DESC_D << 6) + (DESC_L << 5) + (DESC_AVL << 4) + 0x0)    //TSS段描述符高32位高字
#define TSS_ATTR_LOW ((DESC_P << 7) + (DESC_DPL_0 << 5) + (DESC_S_SYS << 4) + DESC_TYPE_TSS)            //TSS段描述符高32位低字
#define SELECTOR_TSS ((4 << 3) + (TI_GDT << 2 ) + RPL0)

struct gdt_desc {
   uint16_t limit_low_word;
   uint16_t base_low_word;
   uint8_t  base_mid_byte;
   uint8_t  attr_low_byte;
   uint8_t  limit_high_attr_high;
   uint8_t  base_high_byte;
}; 

#define PG_SIZE 4096

////定义模块化的中断门描述符attr字段，attr字段指的是中断门描述符高字第8到16bit
#define	 IDT_DESC_P 1 
#define	 IDT_DESC_DPL0 0
#define	 IDT_DESC_DPL3 3
#define	 IDT_DESC_32_TYPE 0xE   // 32位的门
#define	 IDT_DESC_16_TYPE 0x6   // 16位的门，不用，定义它只为和32位门区分


#define	 IDT_DESC_ATTR_DPL0  ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)  //DPL为0的中断门描述符attr字段
#define	 IDT_DESC_ATTR_DPL3  ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)  //DPL为3的中断门描述符attr字段

#define NULL ((void*)0)
#define bool int
#define true 1
#define false 0
#define DIV_ROUND_UP(X, STEP) ((X + STEP - 1) / (STEP))  //用于向上取整的宏，如9/10=1

//定义eflages寄存器用的一些字段，含义见书p511
#define EFLAGS_MBS	(1 << 1)	// 此项必须要设置
#define EFLAGS_IF_1	(1 << 9)	// if为1,开中断
#define EFLAGS_IF_0	0		// if为0,关中断
#define EFLAGS_IOPL_3	(3 << 12)	// IOPL3,用于测试用户程序在非系统调用下进行IO
#define EFLAGS_IOPL_0	(0 << 12)	// IOPL0

#define UNUSED __attribute__ ((unused))

#endif
