#include "memory.h"
#include "stdint.h"
#include "print.h"

#define PG_SIZE 4096    //一页的大小
#define MEM_BITMAP_BASE 0xc009a000  //这个地址是位图的起始地址，1MB内存布局中，9FBFF是最大一段可用区域的边界，而我们计划这个可用空间最后的位置将来用来
        //放PCB，而PCB占用内存是一个自然页，所以起始地址必须是0xxxx000这种形式，离0x9fbff最近的符合这个形式的地址是0x9f000。我们又为了将来可能的拓展，
        // 所以让位图可以支持管理512MB的内存空间，所以预留位图大小为16KB，也就是4页，所以选择0x9a000作为位图的起始地址

//定义内核堆区起始地址，堆区就是用来进行动态内存分配的地方，咱们的系统内核运行在c00000000开始的1MB虚拟地址空间，所以自然要跨过这个空间，
//堆区的起始地址并没有跨过256个页表，没关系，反正使用虚拟地址最终都会被我们的页表转换为物理地址，我们建立物理映射的时候，跳过256个页表就行了
#define K_HEAP_START 0xc0100000

/* 核心数据结构，物理内存池， 生成两个实例用于管理内核物理内存池和用户物理内存池 */
struct pool {
   struct bitmap pool_bitmap;	 // 本内存池用到的位图结构,用于管理物理内存
   uint32_t phy_addr_start;	     // 本内存池所管理物理内存的起始地址
   uint32_t pool_size;		    // 本内存池字节容量
};

struct pool kernel_pool, user_pool;      //为kernel与user分别建立物理内存池，让用户进程只能从user内存池获得新的内存空间，
        //以免申请完所有可用空间,内核就不能申请空间了
struct virtual_addr kernel_vaddr;	 // 用于管理内核虚拟地址空间

//初始化内核物理内存池与用户物理内存池
static void mem_pool_init(uint32_t all_mem) {
   put_str("   mem_pool_init start\n");
   uint32_t page_table_size = PG_SIZE * 256;	  // 页表大小= 1页的页目录表+第0和第768个页目录项指向同一个页表+
                                                  // 第769~1022个页目录项共指向254个页表,共256个页表
   uint32_t used_mem = page_table_size + 0x100000;	  // 已使用内存 = 1MB + 256个页表
   uint32_t free_mem = all_mem - used_mem;
   uint16_t all_free_pages = free_mem / PG_SIZE;        //将所有可用内存转换为页的数量，内存分配以页为单位，丢掉的内存不考虑
   uint16_t kernel_free_pages = all_free_pages / 2;     //可用内存是用户与内核各一半，所以分到的页自然也是一半
   uint16_t user_free_pages = all_free_pages - kernel_free_pages;   //用于存储用户空间分到的页

/* 为简化位图操作，余数不处理，坏处是这样做会丢内存。
好处是不用做内存的越界检查,因为位图表示的内存少于实际物理内存*/
   uint32_t kbm_length = kernel_free_pages / 8;			  // 内核物理内存池的位图长度,位图中的一位表示一页,以字节为单位
   uint32_t ubm_length = user_free_pages / 8;			  // 用户物理内存池的位图长度.

   uint32_t kp_start = used_mem;				  // Kernel Pool start,内核使用的物理内存池的起始地址
   uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;	  // User Pool start,用户使用的物理内存池的起始地址

   kernel_pool.phy_addr_start = kp_start;       //赋值给内核使用的物理内存池的起始地址
   user_pool.phy_addr_start   = up_start;       //赋值给用户使用的物理内存池的起始地址

   kernel_pool.pool_size = kernel_free_pages * PG_SIZE;     //赋值给内核使用的物理内存池的总大小
   user_pool.pool_size	 = user_free_pages * PG_SIZE;       //赋值给用户使用的物理内存池的总大小

   kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;     //赋值给管理内核使用的物理内存池的位图长度
   user_pool.pool_bitmap.btmp_bytes_len	  = ubm_length;   //赋值给管理用户使用的物理内存池的位图长度

/*********    内核内存池和用户内存池位图   ***********
 *   位图是全局的数据，长度不固定。
 *   全局或静态的数组需要在编译时知道其长度，
 *   而我们需要根据总内存大小算出需要多少字节。
 *   所以改为指定一块内存来生成位图.
 *   ************************************************/
// 内核使用的最高地址是0xc009f000,这是主线程的栈地址.(内核的大小预计为70K左右)
// 32M内存占用的位图是2k.内核内存池的位图先定在MEM_BITMAP_BASE(0xc009a000)处.
   kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;      //管理内核使用的物理内存池的位图起始地址
							       
/* 用户内存池的位图紧跟在内核内存池位图之后 */
   user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);     //管理用户使用的物理内存池的位图起始地址
   /******************** 输出内存池信息 **********************/
   put_str("      kernel_pool_bitmap_start:");put_int((int)kernel_pool.pool_bitmap.bits);
   put_str(" kernel_pool_phy_addr_start:");put_int(kernel_pool.phy_addr_start);
   put_str("\n");
   put_str("      user_pool_bitmap_start:");put_int((int)user_pool.pool_bitmap.bits);
   put_str(" user_pool_phy_addr_start:");put_int(user_pool.phy_addr_start);
   put_str("\n");

   /* 将位图置0*/
   bitmap_init(&kernel_pool.pool_bitmap);
   bitmap_init(&user_pool.pool_bitmap);

   /* 下面初始化内核虚拟地址的位图,按实际物理内存大小生成数组。*/
   kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;      // 赋值给管理内核可以动态使用的虚拟地址池（堆区）的位图长度，
         //其大小与管理内核可使用的物理内存池位图长度相同，因为虚拟内存最终都要转换为真实的物理内存，可用虚拟内存大小超过可用物理内存大小在
         //我们这个简单操作系统无意义（现代操作系统中有意义，因为我们可以把真实物理内存不断换出，回收，来让可用物理内存变相变大)

  /* 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外*/
   kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);   //赋值给管理内核可以动态使用的虚拟内存池（堆区）的位图起始地址

   kernel_vaddr.vaddr_start = K_HEAP_START;     //赋值给内核可以动态使用的虚拟地址空间的起始地址
   bitmap_init(&kernel_vaddr.vaddr_bitmap);     //初始化管理内核可以动态使用的虚拟地址池的位图
   put_str("   mem_pool_init done\n");
}

/* 内存管理部分初始化入口 */
void mem_init() {
   put_str("mem_init start\n");
   uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
   mem_pool_init(mem_bytes_total);	  // 初始化内存池
   put_str("mem_init done\n");
}
