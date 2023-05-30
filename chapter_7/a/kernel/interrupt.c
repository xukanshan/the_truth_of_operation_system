#include "interrupt.h"      //里面定义了intr_handler类型
#include "stdint.h"         //各种uint_t类型
#include "global.h"         //里面定义了选择子
#include "io.h"             
#include "print.h"


#define PIC_M_CTRL 0x20	       // 这里用的可编程中断控制器是8259A,主片的控制端口是0x20
#define PIC_M_DATA 0x21	       // 主片的数据端口是0x21
#define PIC_S_CTRL 0xa0	       // 从片的控制端口是0xa0
#define PIC_S_DATA 0xa1	       // 从片的数据端口是0xa1


#define IDT_DESC_CNT 0x21	   //支持的中断描述符个数33

//按照中断门描述符格式定义结构体
struct gate_desc {
   uint16_t    func_offset_low_word;        //函数地址低字
   uint16_t    selector;                    //选择子字段
   uint8_t     dcount;                      //此项为双字计数字段，是门描述符中的第4字节。这个字段无用
   uint8_t     attribute;                   //属性字段
   uint16_t    func_offset_high_word;       //函数地址高字
};

// 静态函数声明,非必须
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];   //中断门描述符（结构体）数组，名字叫idt

extern intr_handler intr_entry_table[IDT_DESC_CNT];	    //引入kernel.s中定义好的中断处理函数地址数组，intr_handler就是void* 表明是一般地址类型



/* 初始化可编程中断控制器8259A */
static void pic_init(void) {

   /* 初始化主片 */
   outb (PIC_M_CTRL, 0x11);   // ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_M_DATA, 0x20);   // ICW2: 起始中断向量号为0x20,也就是IR[0-7] 为 0x20 ~ 0x27.
   outb (PIC_M_DATA, 0x04);   // ICW3: IR2接从片. 
   outb (PIC_M_DATA, 0x01);   // ICW4: 8086模式, 正常EOI

   /* 初始化从片 */
   outb (PIC_S_CTRL, 0x11);	// ICW1: 边沿触发,级联8259, 需要ICW4.
   outb (PIC_S_DATA, 0x28);	// ICW2: 起始中断向量号为0x28,也就是IR[8-15] 为 0x28 ~ 0x2F.
   outb (PIC_S_DATA, 0x02);	// ICW3: 设置从片连接到主片的IR2引脚
   outb (PIC_S_DATA, 0x01);	// ICW4: 8086模式, 正常EOI

   /* 打开主片上IR0,也就是目前只接受时钟产生的中断 */
   outb (PIC_M_DATA, 0xfe);
   outb (PIC_S_DATA, 0xff);

   put_str("   pic_init done\n");
}


//此函数用于将传入的中断门描述符与中断处理函数建立映射，三个参数：中断门描述符地址，属性，中断处理函数地址
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) { 
   p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
   p_gdesc->selector = SELECTOR_K_CODE;
   p_gdesc->dcount = 0;
   p_gdesc->attribute = attr;
   p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

//此函数用来循环调用make_idt_desc函数来完成中断门描述符与中断处理函数映射关系的建立,传入三个参数：中断描述符表某个中段描述符（一个结构体）的地址
//属性字段，中断处理函数的地址
static void idt_desc_init(void) {
   int i;
   for (i = 0; i < IDT_DESC_CNT; i++) {
      make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]); 
   }
   put_str("   idt_desc_init done\n");
}


/*完成有关中断的所有初始化工作*/
void idt_init() {
   put_str("idt_init start\n");
   idt_desc_init();	   //调用上面写好的函数完成中段描述符表的构建
   pic_init();		  //设定化中断控制器，只接受来自时钟中断的信号

   /* 加载idt */
   uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));    //定义要加载到IDTR寄存器中的值
   asm volatile("lidt %0" : : "m" (idt_operand));
   put_str("idt_init done\n");
}

