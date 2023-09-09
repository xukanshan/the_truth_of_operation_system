%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR

                                                
GDT_BASE:                                               ;构建gdt及其内部的描述符
    dd 0x00000000 
	dd 0x00000000

CODE_DESC:  
    dd 0x0000FFFF 
	dd DESC_CODE_HIGH4

DATA_STACK_DESC:  
    dd 0x0000FFFF
    dd DESC_DATA_HIGH4

VIDEO_DESC: 
    dd 0x80000007	                                    ;limit=(0xbffff-0xb8000)/4k=0x7
    dd DESC_VIDEO_HIGH4                                 ; 此时dpl已改为0

    GDT_SIZE equ $ - GDT_BASE
    GDT_LIMIT equ GDT_SIZE - 1 
    times 60 dq 0					                    ; 此处预留60个描述符的空间
    SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0       ; 相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
    SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0	    ; 同上
    SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0	    ; 同上 

total_mem_bytes dd 0				                    ; total_mem_bytes用于保存内存容量,以字节为单位,此位置比较好记。
                                                        ; 当前偏移loader.bin文件头0x200字节,loader.bin的加载地址是0x900,
                                                        ; 故total_mem_bytes内存中的地址是0xb00.将来在内核中咱们会引用此地址	 
                                                        
gdt_ptr dw GDT_LIMIT                                    ;定义加载进入GDTR的数据，前2字节是gdt界限，后4字节是gdt起始地址，
	    dd  GDT_BASE

ards_buf times 244 db 0                                 ;人工对齐:total_mem_bytes4字节+gdt_ptr6字节+ards_buf244字节+ards_nr2,共256字节
ards_nr dw 0		                                    ;用于记录ards结构体数量


loader_start:
                                                        ;-------  int 15h eax = 0000E820h ,edx = 534D4150h ('SMAP') 获取内存布局  -------

    xor ebx, ebx		                                ;第一次调用时，ebx值要为0
    mov edx, 0x534d4150	                                ;edx只赋值一次，循环体中不会改变
    mov di, ards_buf	                                ;ards结构缓冲区
.e820_mem_get_loop:	                                    ;循环获取每个ARDS内存范围描述结构
    mov eax, 0x0000e820	                                ;执行int 0x15后,eax值变为0x534d4150,所以每次执行int前都要更新为子功能号。
    mov ecx, 20		                                    ;ARDS地址范围描述符结构大小是20字节
    int 0x15
    add di, cx		                                    ;使di增加20字节指向缓冲区中新的ARDS结构位置
    inc word [ards_nr]	                                ;记录ARDS数量
    cmp ebx, 0		                                    ;若ebx为0且cf不为1,这说明ards全部返回，当前已是最后一个
    jnz .e820_mem_get_loop

                                                        ;在所有ards结构中，找出(base_add_low + length_low)的最大值，即内存的容量。
    mov cx, [ards_nr]	                                ;遍历每一个ARDS结构体,循环次数是ARDS的数量
    mov ebx, ards_buf 
    xor edx, edx		                                ;edx为最大的内存容量,在此先清0
.find_max_mem_area:	                                    ;无须判断type是否为1,最大的内存块一定是可被使用
    mov eax, [ebx]	                                    ;base_add_low
    add eax, [ebx+8]	                                ;length_low
    add ebx, 20		                                    ;指向缓冲区中下一个ARDS结构
    cmp edx, eax		                                ;冒泡排序，找出最大,edx寄存器始终是最大的内存容量
    jge .next_ards
    mov edx, eax		                                ;edx为总内存大小
.next_ards:
    loop .find_max_mem_area

    mov [total_mem_bytes], edx	                        ;将内存换为byte单位后存入total_mem_bytes处。

                                                        ;-----------------   准备进入保护模式   ------------------------------------------
                                                        ;1 打开A20
                                                        ;2 加载gdt
                                                        ;3 将cr0的pe位置1

                                                        ;-----------------  打开A20  ----------------
    in al, 0x92
    or al, 0000_0010B
    out 0x92,al

                                                         ;-----------------  加载GDT  ----------------
    lgdt [gdt_ptr]


                                                        ;-----------------  cr0第0位置1  ----------------
    mov eax,cr0
    or eax,0x00000001
    mov cr0,eax

                                                        ;jmp dword SELECTOR_CODE:p_mode_start	    
    jmp SELECTOR_CODE:p_mode_start	                    ; 刷新流水线，避免分支预测的影响,这种cpu优化策略，最怕jmp跳转，
					                                    ; 这将导致之前做的预测失效，从而起到了刷新的作用。

.error_hlt:		                                        ;出错则挂起
    hlt

[bits 32]
p_mode_start:
    mov ax,SELECTOR_DATA
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov esp,LOADER_STACK_TOP
    mov ax,SELECTOR_VIDEO
    mov gs,ax

                                                        ; -------------------------   加载kernel  ----------------------
    mov eax, KERNEL_START_SECTOR                        ; kernel.bin所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR                       ; 从磁盘读出后，写入到ebx指定的地址
    mov ecx, 200			                            ; 读入的扇区数
    call rd_disk_m_32
                                                        
    call setup_page                                     ;创建页目录表的函数,我们的页目录表必须放在1M开始的位置，所以必须在开启保护模式后运行

                                                        ;以下两句是将gdt描述符中视频段描述符中的段基址+0xc0000000
    mov ebx, [gdt_ptr + 2]                              ;ebx中存着GDT_BASE
    or dword [ebx + 0x18 + 4], 0xc0000000               ;视频段是第3个段描述符,每个描述符是8字节,故0x18 = 24，然后+4，是取出了视频段段描述符的高4字节。然后or操作，段基址最高位+c
                                           
    add dword [gdt_ptr + 2], 0xc0000000                 ;将gdt的基址加上0xc0000000使其成为内核所在的高地址

    add esp, 0xc0000000                                 ; 将栈指针同样映射到内核地址

    mov eax, PAGE_DIR_TABLE_POS                         ; 把页目录地址赋给cr3
    mov cr3, eax
                                                        
    mov eax, cr0                                        ; 打开cr0的pg位(第31位)
    or eax, 0x80000000  
    mov cr0, eax
                                                      
    lgdt [gdt_ptr]                                      ;在开启分页后,用gdt新的地址重新加载

enter_kernel:    
    call kernel_init
    mov esp, 0xc009f000
    jmp KERNEL_ENTRY_POINT                              ; 用地址0x1500访问测试，结果ok

                                                        ;-----------------   将kernel.bin中的segment拷贝到编译的地址   -----------
kernel_init:
    xor eax, eax                                        ;清空eax
    xor ebx, ebx		                                ;清空ebx, ebx记录程序头表地址
    xor ecx, ecx		                                ;清空ecx, cx记录程序头表中的program header数量
    xor edx, edx		                                ;清空edx, dx 记录program header尺寸

    mov dx, [KERNEL_BIN_BASE_ADDR + 42]	                ; 偏移文件42字节处的属性是e_phentsize,表示program header table中每个program header大小
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]                ; 偏移文件开始部分28字节的地方是e_phoff,表示program header table的偏移，ebx中是第1 个program header在文件中的偏移量
					                                    ; 其实该值是0x34,不过还是谨慎一点，这里来读取实际值
    add ebx, KERNEL_BIN_BASE_ADDR                       ; 现在ebx中存着第一个program header的内存地址
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]                 ; 偏移文件开始部分44字节的地方是e_phnum,表示有几个program header
.each_segment:
    cmp byte [ebx + 0], PT_NULL		                    ; 若p_type等于 PT_NULL,说明此program header未使用。
    je .PTNULL

                                                        ;为函数memcpy压入参数,参数是从右往左依然压入.函数原型类似于 memcpy(dst,src,size)
    push dword [ebx + 16]		                        ; program header中偏移16字节的地方是p_filesz,压入函数memcpy的第三个参数:size
    mov eax, [ebx + 4]			                        ; 距程序头偏移量为4字节的位置是p_offset，该值是本program header 所表示的段相对于文件的偏移
    add eax, KERNEL_BIN_BASE_ADDR	                    ; 加上kernel.bin被加载到的物理地址,eax为该段的物理地址
    push eax				                            ; 压入函数memcpy的第二个参数:源地址
    push dword [ebx + 8]			                    ; 压入函数memcpy的第一个参数:目的地址,偏移程序头8字节的位置是p_vaddr，这就是目的地址
    call mem_cpy				                        ; 调用mem_cpy完成段复制
    add esp,12				                            ; 清理栈中压入的三个参数
.PTNULL:
   add ebx, edx				                            ; edx为program header大小,即e_phentsize,在此ebx指向下一个program header 
   loop .each_segment
   ret

                                                        ;----------  逐字节拷贝 mem_cpy(dst,src,size) ------------
                                                        ;输入:栈中三个参数(dst,src,size)
                                                        ;输出:无
                                                        ;---------------------------------------------------------
mem_cpy:		      
    cld                                                 ;将FLAG的方向标志位DF清零，rep在执行循环时候si，di就会加1
    push ebp                                            ;这两句指令是在进行栈框架构建
    mov ebp, esp
    push ecx		                                    ; rep指令用到了ecx，但ecx对于外层段的循环还有用，故先入栈备份
    mov edi, [ebp + 8]	                                ; dst，edi与esi作为偏移，没有指定段寄存器的话，默认是ss寄存器进行配合
    mov esi, [ebp + 12]	                                ; src
    mov ecx, [ebp + 16]	                                ; size
    rep movsb		                                    ; 逐字节拷贝

                                                        ;恢复环境
    pop ecx		
    pop ebp
    ret



                                                       
setup_page:                                             ;------------------------------------------   创建页目录及页表  -------------------------------------
                                                        ;----------------以下6行是将1M开始的4KB置为0，将页目录表初始化
    mov ecx, 4096                                       ;创建4096个byte 0，循环4096次
    mov esi, 0                                          ;用esi来作为偏移量寻址
.clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir

                                                        ; ----------------初始化页目录表，让0号项与768号指向同一个页表，该页表管理从0开始4M的空间
.create_pde:				                            ;一个页目录表项可表示4MB内存,这样0xc03fffff以下的地址和0x003fffff以下的地址都指向相同的页表，这是为将地址映射为内核地址做准备
    mov eax, PAGE_DIR_TABLE_POS                         ; eax中存着页目录表的位置
    add eax, 0x1000 			                        ; 在页目录表位置的基础上+4K（页目录表的大小），现在eax中第一个页表的起始位置
    mov ebx, eax				                        ; 此处为ebx赋值，现在ebx存着第一个页表的起始位置
    or eax, PG_US_U | PG_RW_W | PG_P	                ; 页目录项的属性RW和P位为1,US为1,表示用户属性,所有特权级别都可以访问.
                                                        ; 现在eax中的值符合一个页目录项的要求了，高20位是一个指向第一个页表的4K整数倍地址，低12位是相关属性设置
    mov [PAGE_DIR_TABLE_POS + 0x0], eax                 ; 页目录表0号项写入第一个页表的位置(0x101000)及属性(7)
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax               ; 页目录表768号项写入第一个页表的位置(0x101000)及属性(7)
					                                    
    sub eax, 0x1000                                     ;----------------- 使最后一个目录项指向页目录表自己的地址，为的是将来动态操作页表做准备
    mov [PAGE_DIR_TABLE_POS + 4092], eax	            ;属性包含PG_US_U是为了将来init进程（运行在用户空间）访问这个页目录表项
                                                        
    mov ecx, 256				                        ; -----------------初始化第一个页表，因为我们的操作系统不会超过1M，所以只用初始化256项
    mov esi, 0                                          ; esi来做寻址页表项的偏移量
    mov edx, PG_US_U | PG_RW_W | PG_P	                ; 属性为7,US=1,RW=1,P=1
.create_pte:				                            ; 创建Page Table Entry
    mov [ebx+esi*4],edx			                        ; 此时的ebx已经在上面通过eax赋值为0x101000,也就是第一个页表的地址 
    add edx,4096                                        ; edx指向下一个4kb空间，且已经设定好了属性，故edx中是一个完整指向下一个4kb物理空间的页表表项
    inc esi                                             ; 寻址页表项的偏移量+1
    loop .create_pte                                    ;循环设定第一个页表的256项

                                                        ; -------------------初始化页目录表769号-1022号项，769号项指向第二个页表的地址（此页表紧挨着上面的第一个页表），770号指向第三个，以此类推
    mov eax, PAGE_DIR_TABLE_POS                         ; eax存页目录表的起始位置
    add eax, 0x2000 		                            ; 此时eax为第二个页表的位置
    or eax, PG_US_U | PG_RW_W | PG_P                    ; 设置页目录表项相关属性，US,RW和P位都为1，现在eax中的值是一个完整的指向第二个页表的页目录表项
    mov ebx, PAGE_DIR_TABLE_POS                         ; ebx现在存着页目录表的起始位置
    mov ecx, 254			                            ; 要设置254个表项
    mov esi, 769                                        ; 要设置的页目录表项的偏移起始
.create_kernel_pde:
    mov [ebx+esi*4], eax                                ; 设置页目录表项
    inc esi                                             ; 增加要设置的页目录表项的偏移
    add eax, 0x1000                                     ; eax指向下一个页表的位置，由于之前设定了属性，所以eax是一个完整的指向下一个页表的页目录表项
    loop .create_kernel_pde                             ; 循环设定254个页目录表项
    ret


                                                        

                                                        ;-------------------------------------------------------------------------------
                                                        ;功能:读取硬盘n个扇区
rd_disk_m_32:	   
                                                        ;-------------------------------------------------------------------------------
				                                        ; eax=LBA扇区号
				                                        ; ebx=将数据写入的内存地址
				                                        ; ecx=读入的扇区数
    mov esi,eax	                                        ;备份eax
    mov di,cx		                                    ;备份cx
                                                        ;读写硬盘:
                                                        ;第1步：选择特定通道的寄存器，设置要读取的扇区数
    mov dx,0x1f2
    mov al,cl
    out dx,al                                           ;读取的扇区数

    mov eax,esi	                                        ;恢复ax

                                                        ;第2步：在特定通道寄存器中放入要读取扇区的地址，将LBA地址存入0x1f3 ~ 0x1f6
                                                        ;LBA地址7~0位写入端口0x1f3
    mov dx,0x1f3                       
    out dx,al                          

                                                        ;LBA地址15~8位写入端口0x1f4
    mov cl,8
    shr eax,cl
    mov dx,0x1f4
    out dx,al

                                                        ;LBA地址23~16位写入端口0x1f5
    shr eax,cl
    mov dx,0x1f5
    out dx,al

    shr eax,cl
    and al,0x0f	                                        ;lba第24~27位
    or al,0xe0	                                        ; 设置7～4位为1110,表示lba模式
    mov dx,0x1f6
    out dx,al

                                                        ;第3步：向0x1f7端口写入读命令，0x20 
    mov dx,0x1f7
    mov al,0x20                        
    out dx,al

                                                        ;第4步：检测硬盘状态
.not_ready:
                                                        ;同一端口，写时表示写入命令字，读时表示读入硬盘状态
    nop
    in al,dx
    and al,0x88	                                        ;第4位为1表示硬盘控制器已准备好数据传输，第7位为1表示硬盘忙
    cmp al,0x08
    jnz .not_ready	                                    ;若未准备好，继续等。

                                                        ;第5步：从0x1f0端口读数据
    mov ax, di                                          ;di当中存储的是要读取的扇区数
    mov dx, 256                                         ;每个扇区512字节，一次读取两个字节，所以一个扇区就要读取256次，与扇区数相乘，就等得到总读取次数
    mul dx                                              ;8位乘法与16位乘法知识查看书p133,注意：16位乘法会改变dx的值！！！！
    mov cx, ax	                                        ; 得到了要读取的总次数，然后将这个数字放入cx中
    mov dx, 0x1f0
.go_on_read:
    in ax,dx
    mov [ebx],ax                                        ;与rd_disk_m_16相比，就是把这两句的bx改成了ebx
    add ebx,2		        
                                                        ; 由于在实模式下偏移地址为16位,所以用bx只会访问到0~FFFFh的偏移。
                                                        ; loader的栈指针为0x900,bx为指向的数据输出缓冲区,且为16位，
                                                        ; 超过0xffff后,bx部分会从0开始,所以当要读取的扇区数过大,待写入的地址超过bx的范围时，
                                                        ; 从硬盘上读出的数据会把0x0000~0xffff的覆盖，
                                                        ; 造成栈被破坏,所以ret返回时,返回地址被破坏了,已经不是之前正确的地址,
                                                        ; 故程序出会错,不知道会跑到哪里去。
                                                        ; 所以改为ebx代替bx指向缓冲区,这样生成的机器码前面会有0x66和0x67来反转。
                                                        ; 0X66用于反转默认的操作数大小! 0X67用于反转默认的寻址方式.
                                                        ; cpu处于16位模式时,会理所当然的认为操作数和寻址都是16位,处于32位模式时,
                                                        ; 也会认为要执行的指令是32位.
                                                        ; 当我们在其中任意模式下用了另外模式的寻址方式或操作数大小(姑且认为16位模式用16位字节操作数，
                                                        ; 32位模式下用32字节的操作数)时,编译器会在指令前帮我们加上0x66或0x67，
                                                        ; 临时改变当前cpu模式到另外的模式下.
                                                        ; 假设当前运行在16位模式,遇到0X66时,操作数大小变为32位.
                                                        ; 假设当前运行在32位模式,遇到0X66时,操作数大小变为16位.
                                                        ; 假设当前运行在16位模式,遇到0X67时,寻址方式变为32位寻址
                                                        ; 假设当前运行在32位模式,遇到0X67时,寻址方式变为16位寻址.
    loop .go_on_read
    ret
