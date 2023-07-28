#include "tss.h"
#include "stdint.h"
#include "global.h"
#include "string.h"
#include "print.h"

//定义tss的数据结构，在内存中tss的分布就是这个结构体
struct tss {
    uint32_t backlink;
    uint32_t* esp0;
    uint32_t ss0;
    uint32_t* esp1;
    uint32_t ss1;
    uint32_t* esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t (*eip) (void);
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trace;
    uint16_t io_base;
};

static struct tss tss;

//用于更新TSS中的esp0的值，让它指向线程/进程的0级栈
void update_tss_esp(struct task_struct* pthread) {
   tss.esp0 = (uint32_t*)((uint32_t)pthread + PG_SIZE);
}

//用于创建gdt描述符，传入参数1，段基址，传入参数2，段界限；参数3，属性低字节，参数4，属性高字节(要把低四位置0，高4位才是属性)
static struct gdt_desc make_gdt_desc(uint32_t* desc_addr, uint32_t limit, uint8_t attr_low, uint8_t attr_high) {
   uint32_t desc_base = (uint32_t)desc_addr;
   struct gdt_desc desc;
   desc.limit_low_word = limit & 0x0000ffff;
   desc.base_low_word = desc_base & 0x0000ffff;
   desc.base_mid_byte = ((desc_base & 0x00ff0000) >> 16);
   desc.attr_low_byte = (uint8_t)(attr_low);
   desc.limit_high_attr_high = (((limit & 0x000f0000) >> 16) + (uint8_t)(attr_high));
   desc.base_high_byte = desc_base >> 24;
   return desc;
}

/* 在gdt中创建tss并重新加载gdt */
void tss_init() {
   put_str("tss_init start\n");
   uint16_t tss_size = (uint16_t)sizeof(tss);
   memset(&tss, 0, tss_size);
   tss.ss0 = SELECTOR_K_STACK;
   tss.io_base = tss_size;    //io_base 字段的值大于或等于 TSS 的大小，那么这意味着 用于表示I/O 位图的数组超出了 TSS 的界限，
                              //或者说，TSS 结构实际上并没有包含 I/O 位图。在这种情况下，处理器就会假定该任务可以访问所有 I/O 端口

/* gdt段基址为0x900,把tss放到第4个位置,也就是0x900+0x20的位置 */

  //在gdt表中添加tss段描述符，在本系统的，GDT表的起始位置为0x00000900，那么tss的段描述就应该在0x920(0x900+十进制4*8)
  *((struct gdt_desc*)0xc0000920) = make_gdt_desc((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);

  /* 在gdt中添加dpl为3的数据段和代码段描述符 */
  *((struct gdt_desc*)0xc0000928) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_CODE_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
  *((struct gdt_desc*)0xc0000930) = make_gdt_desc((uint32_t*)0, 0xfffff, GDT_DATA_ATTR_LOW_DPL3, GDT_ATTR_HIGH);
   
  /* gdt 16位的limit 32位的段基址 */
   uint64_t gdt_operand = ((8 * 7 - 1) | ((uint64_t)(uint32_t)0xc0000900 << 16));   // 7个描述符大小
   asm volatile ("lgdt %0" : : "m" (gdt_operand));
   asm volatile ("ltr %w0" : : "r" (SELECTOR_TSS));
   put_str("tss_init and ltr done\n");
}

