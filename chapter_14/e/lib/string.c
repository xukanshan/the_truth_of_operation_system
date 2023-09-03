#include "string.h"
#include "global.h"
#include "debug.h"  //定义了ASSERT

//将dst起始的size个字节置为value，这个函数最常用的用法就是来初始化一块内存区域，也就是置为ASCII码为0
void memset(void* dst_, uint8_t value, uint32_t size) {
    ASSERT(dst_ != NULL);            //一般开发都有这个习惯，传入进来个地址，判断不是空
    uint8_t* dst = (uint8_t*)dst_;   //强制类型转换，将对地址的操作单位变成一字节
    while (size-- > 0)               //先判断size是否>0，然后再减，然后执行循环体，size是多少，就会循环多少次
        *dst++ = value;               //*的优先级高于++，所以是先对dst指向的地址进行操作(*dst=value)，然后地址+1
}

//将src地址起始处size字节的数据移入dst，用于拷贝内存数据
//src起始是有数据的，所以用const void*，const修饰void*，意味着地址内的数据是只读
void memcpy(void* dst_, const void* src_, uint32_t size) {
    ASSERT(dst_ != NULL && src_ != NULL);
    uint8_t* dst = dst_;
    const uint8_t* src = src_;
    while (size-- > 0)
        *dst++ = *src++;
}

//比较两个地址起始的size字节的数据是否相等，如果相等，则返回0；如果不相等，比较第一个不相等的数据，>返回1，<返回-1
int memcmp(const void* a_, const void* b_, uint32_t size) {
    const char* a = a_;
    const char* b = b_;
    ASSERT(a != NULL || b != NULL);
    while (size-- > 0) {
        if(*a != *b) {
	        return *a > *b ? 1 : -1; 
        }
    a++;
    b++;
    }
   return 0;
}

//将字符串从src拷贝到dst,并返回目的字符串的起始地址
char* strcpy(char* dst_, const char* src_) {
    ASSERT(dst_ != NULL && src_ != NULL);
    char* r = dst_;		       // 用来返回目的字符串起始地址
    while((*dst_++ = *src_++));  //1、*dst=*src  2、判断*dst是否为'\0'，然后决定是否执行循环体，本步骤真假值不影响3   3、dst++与scr++，谁先谁后不知道
    return r;                    //上面多出来的一对括号，是为了告诉编译器，我这里的=就是自己写的，而不是将==错误写成了=
}

/* 返回字符串长度 */
uint32_t strlen(const char* str) {
    ASSERT(str != NULL);
    const char* p = str;
    while(*p++);                 //1、先取*p的值来进行2的判断     2、判断*p,决定是否执行循环体     3、p++(这一步的执行并不依赖2的判断为真) 
    return (p - str - 1);        //p最后指向'\0'后面第一个元素
}

//比较两个字符串，若a_中的字符与b_中的字符全部相同，则返回0，如果不同，那么比较第一个不同的字符，如果a_>b_返回1，反之返回-1
int8_t strcmp (const char* a, const char* b) {
    ASSERT(a != NULL && b != NULL);
    while (*a != 0 && *a == *b) {
        a++;
        b++;
    }
/* 如果*a小于*b就返回-1,否则就属于*a大于等于*b的情况。在后面的布尔表达式"*a > *b"中,
 * 若*a大于*b,表达式就等于1,否则就表达式不成立,也就是布尔值为0,恰恰表示*a等于*b */
    return *a < *b ? -1 : *a > *b;
}

/* 从左到右查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strchr(const char* str, const uint8_t ch) {
    ASSERT(str != NULL);
    while (*str != 0) {
        if (*str == ch) {
	        return (char*)str;	    // 需要强制转化成和返回值类型一样,否则编译器会报const属性丢失,下同.
        }
        str++;
    }
    return NULL;
}

/* 从后往前查找字符串str中首次出现字符ch的地址(不是下标,是地址) */
char* strrchr(const char* str, const uint8_t ch) {
    ASSERT(str != NULL);
    const char* last_char = NULL;
    /* 从头到尾遍历一次,若存在ch字符,last_char总是该字符最后一次出现在串中的地址(不是下标,是地址)*/
    while (*str != 0) {
        if (*str == ch) {
	        last_char = str;
        }
        str++;
    }
    return (char*)last_char;
}

/* 将字符串src_拼接到dst_后,将回拼接的串地址 */
char* strcat(char* dst_, const char* src_) {
    ASSERT(dst_ != NULL && src_ != NULL);
    char* str = dst_;
    while (*str++);
    --str;                       // 别看错了，--str是独立的一句，并不是while的循环体。这一句是为了让str指向dst_的最后一个非0字符
    while((*str++ = *src_++));	//1、*str=*src  2、判断*str     3、str++与src++，这一步不依赖2
    return dst_;
}

/* 在字符串str中查找指定字符ch出现的次数 */
uint32_t strchrs(const char* str, uint8_t ch) {
    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    const char* p = str;
    while(*p != 0) {
        if (*p == ch) {
            ch_cnt++;
        }
        p++;
    }
    return ch_cnt;
}
