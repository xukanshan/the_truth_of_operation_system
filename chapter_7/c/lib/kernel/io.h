#ifndef __LIB_IO_H
#define __LIB_IO_H
#include "stdint.h"

 //一次送一字节的数据到指定端口，static指定只在本.h内有效，inline是让处理器将函数编译成内嵌的方式，就是在该函数调用处原封不动地展开
 //此函数有两个参数，一个端口号，一个要送往端口的数据
static inline void outb(uint16_t port, uint8_t data) {
/*********************************************************
 a表示用寄存器al或ax或eax,对端口指定N表示0~255, d表示用dx存储端口号, 
 %b0表示对应al,%w1表示对应dx */ 
   asm volatile ( "outb %b0, %w1" : : "a" (data), "Nd" (port));    
}

//利用outsw（端口输出串，一次一字）指令，将ds:esi指向的addr处起始的word_cnt(存在ecx中)个字写入端口port,ecx与esi会自动变化
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {
/*********************************************************
   +表示此限制即做输入又做输出.
   outsw是把ds:esi处的16位的内容写入port端口, 我们在设置段描述符时, 
   已经将ds,es,ss段的选择子都设置为相同的值了,此时不用担心数据错乱。*/
   asm volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
}                                       //S表示寄存器esi/si

/* 将从端口port读入的一个字节返回 */
static inline uint8_t inb(uint16_t port) {
   uint8_t data;
   asm volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
   return data;
}

/* 将从端口port读入的word_cnt个字写入addr */
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
/******************************************************
   insw是将从端口port处读入的16位内容写入es:edi指向的内存,
   我们在设置段描述符时, 已经将ds,es,ss段的选择子都设置为相同的值了,
   此时不用担心数据错乱。*/
   asm volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
}                                   //D表示寄存器edi/di                       //通知编译器，内存已经被改变了

#endif
