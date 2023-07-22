#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1
struct bitmap {                 //这个数据结构就是用来管理整个位图
   uint32_t btmp_bytes_len;     //记录整个位图的大小，字节为单位
   uint8_t* bits;               //用来记录位图的起始地址，我们未来用这个地址遍历位图时，操作单位指定为最小的字节
};

void bitmap_init(struct bitmap* btmp);
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value);
#endif
