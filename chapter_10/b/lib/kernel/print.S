TI_GDT equ  0                                               ;从这里开始三步是在定义显存段段描述符的选择子
RPL0  equ   0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

section .data
put_int_buffer dq 0                                         ; 定义8字节缓冲区用于数字到字符的转换


[bits 32]
section .text
                                                            ;--------------------------------------------
                                                            ;put_str 通过put_char来打印以0字符结尾的字符串
                                                            ;--------------------------------------------
                                                            ;输入：栈中参数为打印的字符串
                                                            ;输出：无

global put_str
put_str:
                                                            ;由于本函数中只用到了ebx和ecx,只备份这两个寄存器
   push ebx
   push ecx
   xor ecx, ecx		                                        ; 准备用ecx存储参数,清空
   mov ebx, [esp + 12]	                                    ; 从栈中得到待打印的字符串地址 
.goon:
   mov cl, [ebx]                                            ;ebx是字符串的地址，对地址进行取地址操作，然后取出一字节的数据，就是取出了字符串的第一个字符
   cmp cl, 0		                                        ; 如果处理到了字符串尾,跳到结束处返回
   jz .str_over
   push ecx		                                            ; 为put_char函数传递参数
   call put_char
   add esp, 4		                                        ; 回收参数所占的栈空间
   inc ebx		                                            ; 使ebx指向下一个字符
   jmp .goon
.str_over:
   pop ecx
   pop ebx
   ret


[bits 32]                                                   ;采用32位编译
section .text                                               ;表明这是个代码段
                                                            ;------------------------   put_char   -----------------------------
                                                            ;功能描述:把栈中的1个字符写入光标所在处
                                                            ;-------------------------------------------------------------------   
global put_char                                             ;将put_char导出为全局符号，这样其他文件也可以使用
put_char:
    pushad	                                                ;备份32位寄存器环境
                                                                ;需要保证gs中为正确的视频段选择子,为保险起见,每次打印时都为gs赋值
    mov ax, SELECTOR_VIDEO	                                ; 不能直接把立即数送入段寄存器
    mov gs, ax

                                                            ;;;;;;;;;  获取当前光标位置 ;;;;;;;;;
                                                            ;先获得高8位
    mov dx, 0x03d4                                          ;索引寄存器
    mov al, 0x0e	                                        ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5                                          ;通过读写数据端口0x3d5来获得或设置光标位置 
    in al, dx	                                            ;得到了光标位置的高8位
    mov ah, al

                                                            ;再获取低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5 
    in al, dx                                                  
    mov bx, ax	                                            ;现在bx中存的是光标的位置
                                                            ;下面这行是在栈中获取待打印的字符
    mov ecx, [esp + 36]	                                    ;pushad压入4×8＝32字节,加上主调函数的返回地址4字节,故esp+36字节,现在ecx中是要打印的字符
    cmp cl, 0xd				                                ;判断是否是CR(回车)0x0d
    jz .is_carriage_return
    cmp cl, 0xa                                             ;判断是否是LF(换行)0x0a
    jz .is_line_feed

    cmp cl, 0x8				                                ;判断是否是BS(backspace退格)的asc码8
    jz .is_backspace
    jmp .put_other	    

.is_backspace:		      
                                                            ;;;;;;;;;;;;       backspace的一点说明	     ;;;;;;;;;;
                                                            ; 当为backspace时,本质上只要将光标移向前一个显存位置即可.后面再输入的字符自然会覆盖此处的字符
                                                            ; 但有可能在键入backspace后并不再键入新的字符,这时在光标已经向前移动到待删除的字符位置,但字符还在原处,
                                                            ; 这就显得好怪异,所以此处添加了空格或空字符0
    dec bx                                                  ;光标位置-1, 以符合我们的常识认知, 即按下退格符, 光标回退
    shl bx,1                                                ;光标的位置就转换成了对应字符的显存位置的偏移
    mov byte [gs:bx], 0x20		                            ;将待删除的字节补为0或空格皆可, 0x20是空格符的ascii码值 
    inc bx                                                  ;bx+1, 指向这个字符的属性位置, 也就是设定背景色, 字符颜色
    mov byte [gs:bx], 0x07                                  ;0x07, 就是黑底白字
    shr bx,1                                                ;bx虽然指向这个字符的颜色属性字节，但是除以2还是变回这个字符的光标位置
    jmp .set_cursor                                         ;去设置光标位置, 这样光标位置才能真正在视觉上更新

 .put_other:
    shl bx, 1				                                ; 光标位置是用2字节表示,将光标值乘2,表示对应显存中的偏移字节
    mov [gs:bx], cl			                                ; ascii字符本身
    inc bx
    mov byte [gs:bx],0x07		                            ; 字符属性
    shr bx, 1				                                ; 恢复老的光标值
    inc bx				                                    ; 下一个光标值
    cmp bx, 2000		   
    jl .set_cursor			                                ; 若光标值小于2000,表示未写到显存的最后,则去设置新的光标值
					                                        ; 若超出屏幕字符数大小(2000)则换行处理
.is_line_feed:				                                ; 是换行符LF(\n)
.is_carriage_return:			                            ; 是回车符CR(\r)
					                                        ; 如果是CR(\r),只要把光标移到行首就行了。
    xor dx, dx				                                ;要进行16位除法，高16位置会放在dx中，要先清零
    mov ax, bx				                                ;ax是被除数的低16位.
    mov si, 80				                                ;用si寄存器来存储除数80 由于是效仿linux，linux中\n便表示下一行的行首，所以本系统中，
    div si				                                    ; 把\n和\r都处理为linux中\n的意思，也就是下一行的行首。ax/80后，ax中存商，dx中存储的是余数,汇编除法https://blog.csdn.net/loovejava/article/details/7044242
    sub bx, dx				                                ; 光标值减去除80的余数便是取整
					                                        ; 以上4行处理\r的代码

 .is_carriage_return_end:		                            ; 回车符CR处理结束
    add bx, 80
    cmp bx, 2000
 .is_line_feed_end:			                                ; 若是LF(\n),将光标移+80便可。  
    jl .set_cursor

                                                            ;屏幕行范围是0~24,滚屏的原理是将屏幕的1~24行搬运到0~23行,再将第24行用空格填充
.roll_screen:				                                ; 若超出屏幕大小，开始滚屏
    cld                                                     
    mov ecx, 960				                            ; 一共有2000-80=1920个字符要搬运,共1920*2=3840字节.一次搬4字节,共3840/4=960次 
    mov esi, 0xb80a0			                            ; 第1行行首
    mov edi, 0xb8000			                            ; 第0行行首
    rep movsd				                                ;rep movs word ptr es:[edi], word ptr ds:[esi] 简写为: rep movsw

                                                            ;将最后一行填充为空白
    mov ebx, 3840			                                ; 最后一行首字符的第一个字节偏移= 1920 * 2
    mov ecx, 80				                                ;一行是80字符(160字节),每次清空1字符(2字节),一行需要移动80次
 .cls:
    mov word [gs:ebx], 0x0720		                        ;0x0720是黑底白字的空格键
    add ebx, 2
    loop .cls 
    mov bx,1920				                                ;将光标值重置为1920,最后一行的首字符.

.set_cursor:   
					                                        ;将光标设为bx值
                                                            ;;;;;;; 1 先设置高8位 ;;;;;;;;
    mov dx, 0x03d4			                                ;索引寄存器
    mov al, 0x0e				                            ;用于提供光标位置的高8位
    out dx, al
    mov dx, 0x03d5			                                ;通过读写数据端口0x3d5来获得或设置光标位置 
    mov al, bh
    out dx, al

                                                            ;;;;;;; 2 再设置低8位 ;;;;;;;;;
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5 
    mov al, bl
    out dx, al
.put_char_done: 
    popad
    ret

                                                            ;--------------------   将小端字节序的数字变成对应的ascii后，倒置   -----------------------
                                                            ;输入：栈中参数为待打印的数字
                                                            ;输出：在屏幕上打印16进制数字,并不会打印前缀0x,如打印10进制15时，只会直接打印f，不会是0xf
                                                            ;------------------------------------------------------------------------------------------
global put_int
put_int:
   pushad
   mov ebp, esp
   mov eax, [ebp+4*9]		                                ; call的返回地址占4字节+pushad的8个4字节，现在eax中就是要显示的32位数值
   mov edx, eax                                             ;ebx中现在是要显示的32位数值
   mov edi, 7                                               ; 指定在put_int_buffer中初始的偏移量，也就是把栈中第一个字节取出放入buffer最后一个位置，第二个字节放入buff倒数第二个位置
   mov ecx, 8			                                    ; 32位数字中,16进制数字的位数是8个
   mov ebx, put_int_buffer                                  ;ebx现在存储的是buffer的起始地址

                                                            ;将32位数字按照16进制的形式从低位到高位逐个处理,共处理8个16进制数字
.16based_4bits:			                                    ; 每4位二进制是16进制数字的1位,遍历每一位16进制数字
   and edx, 0x0000000F		                                ; 解析16进制数字的每一位。and与操作后,edx只有低4位有效
   cmp edx, 9			                                    ; 数字0～9和a~f需要分别处理成对应的字符
   jg .is_A2F 
   add edx, '0'			                                    ; ascii码是8位大小。add求和操作后,edx低8位有效。
   jmp .store
.is_A2F:
   sub edx, 10			                                    ; A~F 减去10 所得到的差,再加上字符A的ascii码,便是A~F对应的ascii码
   add edx, 'A'

                                                            ;将每一位数字转换成对应的字符后,按照类似“大端”的顺序存储到缓冲区put_int_buffer
                                                            ;高位字符放在低地址,低位字符要放在高地址,这样和大端字节序类似,只不过咱们这里是字符序.
.store:
   mov [ebx+edi], dl		                                ; 此时dl中是数字对应的字符的ascii码
   dec edi                                                  ;edi是表示在buffer中存储的偏移，现在向前移动
   shr eax, 4                                               ;eax中是完整存储了这个32位数值，现在右移4位，处理下一个4位二进制表示的16进制数字
   mov edx, eax                                             ;把eax中的值送入edx，让ebx去处理
   loop .16based_4bits

                                                            ;现在put_int_buffer中已全是字符,打印之前,
                                                            ;把高位连续的字符去掉,比如把字符00000123变成123
.ready_to_print:
   inc edi			                                        ; 此时edi退减为-1(0xffffffff),加1使其为0
.skip_prefix_0:                                             ;跳过前缀的连续多个0
   cmp edi,8			                                    ; 若已经比较第9个字符了，表示待打印的字符串为全0 
   je .full0 
                                                            ;找出连续的0字符, edi做为非0的最高位字符的偏移
.go_on_skip:   
   mov cl, [put_int_buffer+edi]
   inc edi
   cmp cl, '0' 
   je .skip_prefix_0		                                ; 继续判断下一位字符是否为字符0(不是数字0)
   dec edi			                                        ;edi在上面的inc操作中指向了下一个字符,若当前字符不为'0',要恢复edi指向当前字符		       
   jmp .put_each_num

.full0:
   mov cl,'0'			                                    ; 输入的数字为全0时，则只打印0
.put_each_num:
   push ecx			                                        ; 此时cl中为可打印的字符
   call put_char
   add esp, 4
   inc edi			                                        ; 使edi指向下一个字符
   mov cl, [put_int_buffer+edi]	                            ; 获取下一个字符到cl寄存器
   cmp edi,8                                                ;当edi=8时，虽然不会去打印，但是实际上已经越界访问缓冲区了
   jl .put_each_num
   popad
   ret

global set_cursor
set_cursor:
   pushad
   mov bx, [esp+36]
															;;;;;;; 1 先设置高8位 ;;;;;;;;
   mov dx, 0x03d4			  								;索引寄存器
   mov al, 0x0e				  								;用于提供光标位置的高8位
   out dx, al
   mov dx, 0x03d5			  								;通过读写数据端口0x3d5来获得或设置光标位置 
   mov al, bh
   out dx, al

															;;;;;;; 2 再设置低8位 ;;;;;;;;;
   mov dx, 0x03d4
   mov al, 0x0f
   out dx, al
   mov dx, 0x03d5 
   mov al, bl
   out dx, al
   popad
   ret