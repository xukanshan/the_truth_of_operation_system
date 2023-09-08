#include "bitmap.h"     //不仅是为了通过一致性检查，位图的数据结构struct bitmap也在这里面
#include "stdint.h"     
#include "string.h"     //里面包含了内存初始化函数，memset
#include "print.h"
#include "interrupt.h"
#include "debug.h"      //ASSERT

/* 将位图btmp初始化 */
void bitmap_init(struct bitmap* btmp) {
   memset(btmp->bits, 0, btmp->btmp_bytes_len);   
}

//用来确定位图的某一位是1，还是0。若是1，返回真（返回的值不一定是1）。否则，返回0。传入两个参数，指向位图的指针，与要判断的位的偏移
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
   uint32_t byte_idx = bit_idx / 8;    //确定要判断的位所在字节的偏移
   uint32_t bit_odd  = bit_idx % 8;    //确定要判断的位在某个字节中的偏移
   return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

//用来在位图中找到cnt个连续的0，以此来分配一块连续未被占用的内存，参数有指向位图的指针与要分配的内存块的个数cnt
//成功就返回起始位的偏移（如果把位图看做一个数组，那么也可以叫做下标），不成功就返回-1
int bitmap_scan(struct bitmap* bitmap, uint32_t cnt){
    uint32_t area_start = 0, area_size = 0;    //用来存储一个连续为0区域的起始位置, 存储一个连续为0的区域大小
    while(1){                   
        while( bitmap_scan_test(bitmap, area_start) && area_start / 8 < bitmap->btmp_bytes_len) //当这个while顺利结束1、area_start就是第一个0的位置；2、area_start已经越过位图边界
            area_start++;
        if(area_start / 8 >= bitmap->btmp_bytes_len)    //上面那个循环跑完可能是area_start已经越过边界，说明此时位图中是全1，那么就没有可分配内存
            return -1;
        area_size = 1;  //来到了这一句说明找到了位图中第一个0，那么此时area_size自然就是1
        while( area_size < cnt ){
            if( (area_start + area_size) / 8 < bitmap->btmp_bytes_len ){    //确保下一个要判断的位不超过边界
                if( bitmap_scan_test(bitmap, area_start + area_size) == 0 ) //判断区域起始0的下一位是否是0
                    area_size++;
                else
                    break;  //进入else，说明下一位是1，此时area_size还没有到达cnt的要求，且一片连续为0的区域截止，break
            }
            else
                return -1;  //来到这里面，说面下一个要判断的位超过边界，且area_size<cnt，返回-1
        }
        if(area_size == cnt)    //有两种情况另上面的while结束，1、area_size == cnt；2、break；所以要判断
            return area_start;
        area_start += (area_size+1); //更新area_start，判断后面是否有满足条件的连续0区域
    }
}

//将位图某一位设定为1或0，传入参数是指向位图的指针与这一位的偏移，与想要的值
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
   ASSERT((value == 0) || (value == 1));
   uint32_t byte_idx = bit_idx / 8;    //确定要设置的位所在字节的偏移
   uint32_t bit_odd  = bit_idx % 8;    //确定要设置的位在某个字节中的偏移

/* 一般都会用个0x1这样的数对字节中的位操作,
 * 将1任意移动后再取反,或者先取反再移位,可用来对位置0操作。*/
   if (value) {		      // 如果value为1
      btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
   } else {		      // 若为0
      btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
   }
}

