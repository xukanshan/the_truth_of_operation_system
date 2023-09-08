#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H
void panic_spin(char* filename, int line, const char* func, const char* condition);

//...是可变参数，也就是随便你传多少个参数，然后原封不动地传到__VA_ARGS_那里去
//__FILE__,__LINE__,__func__是预定义宏，代表这个宏所在的文件名，行数，与函数名字，编译器处理
#define PANIC(...) panic_spin (__FILE__, __LINE__, __func__, __VA_ARGS__)

//如果定义了NDEBUG,那么下面定义的ASSERT就是个空。这样我们可以便捷的让所有ASSERT宏失效。因为有时候断言太多，程序会运行
//很慢。我们如果不想要ASSERT起作用，编译时用gcc-DNDEBUG就行了
#ifdef NDEBUG
   #define ASSERT(CONDITION) ((void)0)
#else
#define ASSERT(CONDITION)   \
    if(CONDITION){}         \
    else{PANIC(#CONDITION);}    //加#后，传入的参数变成字符串

#endif  //结束#ifdef NDEBUG
#endif  //结束#define __KERNEL_DEBUG_H
