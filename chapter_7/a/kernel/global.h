#ifndef __KERNEL_GLOBAL_H
#define __KERNEL_GLOBAL_H
#include "stdint.h"

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

////定义模块化的中断门描述符attr字段，attr字段指的是中断门描述符高字第8到16bit
#define	 IDT_DESC_P 1 
#define	 IDT_DESC_DPL0 0
#define	 IDT_DESC_DPL3 3
#define	 IDT_DESC_32_TYPE 0xE   // 32位的门
#define	 IDT_DESC_16_TYPE 0x6   // 16位的门，不用，定义它只为和32位门区分


#define	 IDT_DESC_ATTR_DPL0  ((IDT_DESC_P << 7) + (IDT_DESC_DPL0 << 5) + IDT_DESC_32_TYPE)  //DPL为0的中断门描述符attr字段
#define	 IDT_DESC_ATTR_DPL3  ((IDT_DESC_P << 7) + (IDT_DESC_DPL3 << 5) + IDT_DESC_32_TYPE)  //DPL为3的中断门描述符attr字段

#endif
