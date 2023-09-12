[bits 32]
%define ERROR_CODE nop		                ; 有些中断进入前CPU会自动压入错误码（32位）,为保持栈中格式统一,这里不做操作.
%define ZERO push 0		                    ; 有些中断进入前CPU不会压入错误码，对于这类中断，我们为了与前一类中断统一管理，就自己压入32位的0

extern idt_table		                    ;idt_table是C中注册的中断处理程序数组

section .data
global intr_entry_table
intr_entry_table:                           ;编译器会将之后所有同属性的section合成一个大的segment，所以这个标号后面会聚集所有的中断处理程序的地址

%macro VECTOR 2                             ;汇编中的宏用法见书p320
section .text                               ;中断处理函数的代码段
intr%1entry:		                        ; 每个中断处理程序都要压入中断向量号,所以一个中断类型一个中断处理程序，自己知道自己的中断向量号是多少,此标号来表示中断处理程序的入口
    %2                                      ;这一步是根据宏传入参数的变化而变化的
                                            
    push ds                                 ; 以下是保存上下文环境
    push es
    push fs
    push gs
    pushad

                                            ; 如果是从片上进入的中断,除了往从片上发送EOI外,还要往主片上发送EOI 
    mov al,0x20                             ; 中断结束命令EOI
    out 0xa0,al                             ;向主片发送OCW2,其中EOI位为1，告知结束中断，详见书p317
    out 0x20,al                             ;向从片发送OCW2,其中EOI位为1，告知结束中断

    push %1			                        ; 不管idt_table中的目标程序是否需要参数,都一律压入中断向量号,调试时很方便
    call [idt_table + %1*4]                 ; 调用idt_table中的C版本中断处理函数
    jmp intr_exit

section .data                               ;这个段就是存的此中断处理函数的地址
    dd    intr%1entry	                    ; 存储各个中断入口程序的地址，形成intr_entry_table数组,定义的地址是4字节，32位
%endmacro

section .text
global intr_exit
intr_exit:	     
                                            ; 以下是恢复上下文环境
    add esp, 4			                    ; 跳过中断号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4			                    ;对于会压入错误码的中断会抛弃错误码（这个错误码是执行中断处理函数之前CPU自动压入的），对于不会压入错误码的中断，就会抛弃上面push的0                    
    iretd				                    ; 从中断返回,32位下iret等同指令iretd


VECTOR 0x00,ZERO                            ;调用之前写好的宏来批量生成中断处理函数，传入参数是中断号码与上面中断宏的%2步骤，这个步骤是什么都不做，还是压入0看p303
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 
VECTOR 0x20,ZERO	;时钟中断对应的入口
VECTOR 0x21,ZERO	;键盘中断对应的入口
VECTOR 0x22,ZERO	;级联用的
VECTOR 0x23,ZERO	;串口2对应的入口
VECTOR 0x24,ZERO	;串口1对应的入口
VECTOR 0x25,ZERO	;并口2对应的入口
VECTOR 0x26,ZERO	;软盘对应的入口
VECTOR 0x27,ZERO	;并口1对应的入口
VECTOR 0x28,ZERO	;实时时钟对应的入口
VECTOR 0x29,ZERO	;重定向
VECTOR 0x2a,ZERO	;保留
VECTOR 0x2b,ZERO	;保留
VECTOR 0x2c,ZERO	;ps/2鼠标
VECTOR 0x2d,ZERO	;fpu浮点单元异常
VECTOR 0x2e,ZERO	;硬盘
VECTOR 0x2f,ZERO	;保留

;;;;;;;;;;;;;;;;   0x80号中断   ;;;;;;;;;;;;;;;;
[bits 32]
extern syscall_table            ;如同之前我们中断处理机制中引入了C中定义的中断处理程序入口地址表一样，这里引入了C中定义的系统调用函数入口地址表
section .text
global syscall_handler
syscall_handler:
                                ;1 保存上下文环境，为了复用之前写好的intr_exit:，所以我们仿照中断处理机制压入的东西，构建系统调用压入的东西
   push 0			            ; 压入0, 使栈中格式统一
   push ds
   push es
   push fs
   push gs
   pushad			            ; PUSHAD指令压入32位寄存器，其入栈顺序是:EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI  
   push 0x80			        ; 此位置压入0x80也是为了保持统一的栈格式

                                ;2 为系统调用子功能传入参数，由于这个函数是3个参数的用户程序系统调用入口都会使用
                                ; 所以我们为了格式统一，直接按照最高参数数量压入3个参数
   push edx			            ; 系统调用中第3个参数
   push ecx			            ; 系统调用中第2个参数
   push ebx			            ; 系统调用中第1个参数

                                ;3 调用c中定义的功能处理函数
   call [syscall_table + eax*4]	    ; 编译器会在栈中根据C函数声明匹配正确数量的参数
   add esp, 12			        ; 跨过上面的三个参数

                                ;4 将call调用后的返回值存入待当前内核栈中eax的位置，c语言会自动把返回值放入eax中（c语言的ABI规定）
   mov [esp + 8*4], eax	
   jmp intr_exit		        ; intr_exit返回,恢复上下文
