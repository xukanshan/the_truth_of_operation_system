#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"
#include "ioqueue.h"

#define KBD_BUF_PORT 0x60   //键盘buffer寄存器端口号为0x60

#define esc '\033'		    //esc 和 delete都没有\转义字符这种形式，用8进制代替
#define delete '\0177'
#define enter '\r'
#define tab '\t'
#define backspace '\b'

#define char_invisible 0    //功能性 不可见字符均设置为0
#define ctrl_l_char char_invisible
#define ctrl_r_char char_invisible 
#define shift_l_char char_invisible
#define shift_r_char char_invisible
#define alt_l_char char_invisible
#define alt_r_char char_invisible
#define caps_lock_char char_invisible

///定义控制字符的通码和断码
#define shift_l_make 0x2a
#define shift_r_make 0x36
#define alt_l_make 0x38
#define alt_r_make 0xe038
#define alt_r_break 0xe0b8
#define ctrl_l_make 0x1d
#define ctrl_r_make 0xe01d
#define ctrl_r_break 0xe09d
#define caps_lock_make 0x3a


//二维数组，用于记录从0x00到0x3a通码对应的按键的两种情况（如0x02，不加shift表示1，加了shift表示！）的ascii码值
//如果没有，则用ascii0替代
char keymap[][2] = {
/* 0x00 */	{0,	0},		
/* 0x01 */	{esc,	esc},		
/* 0x02 */	{'1',	'!'},		
/* 0x03 */	{'2',	'@'},		
/* 0x04 */	{'3',	'#'},		
/* 0x05 */	{'4',	'$'},		
/* 0x06 */	{'5',	'%'},		
/* 0x07 */	{'6',	'^'},		
/* 0x08 */	{'7',	'&'},		
/* 0x09 */	{'8',	'*'},		
/* 0x0A */	{'9',	'('},		
/* 0x0B */	{'0',	')'},		
/* 0x0C */	{'-',	'_'},		
/* 0x0D */	{'=',	'+'},		
/* 0x0E */	{backspace, backspace},	
/* 0x0F */	{tab,	tab},		
/* 0x10 */	{'q',	'Q'},		
/* 0x11 */	{'w',	'W'},		
/* 0x12 */	{'e',	'E'},		
/* 0x13 */	{'r',	'R'},		
/* 0x14 */	{'t',	'T'},		
/* 0x15 */	{'y',	'Y'},		
/* 0x16 */	{'u',	'U'},		
/* 0x17 */	{'i',	'I'},		
/* 0x18 */	{'o',	'O'},		
/* 0x19 */	{'p',	'P'},		
/* 0x1A */	{'[',	'{'},		
/* 0x1B */	{']',	'}'},		
/* 0x1C */	{enter,  enter},
/* 0x1D */	{ctrl_l_char, ctrl_l_char},
/* 0x1E */	{'a',	'A'},		
/* 0x1F */	{'s',	'S'},		
/* 0x20 */	{'d',	'D'},		
/* 0x21 */	{'f',	'F'},		
/* 0x22 */	{'g',	'G'},		
/* 0x23 */	{'h',	'H'},		
/* 0x24 */	{'j',	'J'},		
/* 0x25 */	{'k',	'K'},		
/* 0x26 */	{'l',	'L'},		
/* 0x27 */	{';',	':'},		
/* 0x28 */	{'\'',	'"'},		
/* 0x29 */	{'`',	'~'},		
/* 0x2A */	{shift_l_char, shift_l_char},	
/* 0x2B */	{'\\',	'|'},		
/* 0x2C */	{'z',	'Z'},		
/* 0x2D */	{'x',	'X'},		
/* 0x2E */	{'c',	'C'},		
/* 0x2F */	{'v',	'V'},		
/* 0x30 */	{'b',	'B'},		
/* 0x31 */	{'n',	'N'},		
/* 0x32 */	{'m',	'M'},		
/* 0x33 */	{',',	'<'},		
/* 0x34 */	{'.',	'>'},		
/* 0x35 */	{'/',	'?'},
/* 0x36	*/	{shift_r_char, shift_r_char},	
/* 0x37 */	{'*',	'*'},    	
/* 0x38 */	{alt_l_char, alt_l_char},
/* 0x39 */	{' ',	' '},		
/* 0x3A */	{caps_lock_char, caps_lock_char}
};

int ctrl_status = 0;        //用于记录是否按下ctrl键
int shift_status = 0;       //用于记录是否按下shift
int alt_status = 0;         //用于记录是否按下alt键
int caps_lock_status = 0;   //用于记录是否按下大写锁定
int ext_scancode = 0;       //用于记录是否是扩展码

struct ioqueue kbd_buf;	   // 定义键盘缓冲区

static void intr_keyboard_handler(void)
{
    int break_code;     //用于判断传入值是否是断码
    uint16_t scancode = inb(KBD_BUF_PORT);  //从8042的0x60取出码值
    if(scancode == 0xe0)	//如果传入是0xe0，说明是处理两字节按键的扫描码，那么就应该立即退出去取出下一个字节
    {
    	ext_scancode = 1;   //打开标记，记录传入的是两字节扫描码
    	return;     //退出
    }
    if(ext_scancode)    //如果能进入这个if，那么ext_scancode==1，说明上次传入的是两字节按键扫描码的第一个字节
    {
        scancode =( (0xe000) | (scancode) );    //合并扫描码，这样两字节的按键的扫描码就得到了完整取出
        ext_scancode = 0;   //关闭记录两字节扫描码的标志
    }

    break_code =( (scancode & 0x0080) != 0);  //断码=通码+0x80，如果是断码，那么&出来结果！=0，那么break_code值为1
    if(break_code)  //如果是断码，就要判断是否是控制按键的断码，如果是，就要将表示他们按下的标志清零，如果不是，就不处理。最后都要退出程序
    {
    	uint16_t make_code = (scancode &= 0xff7f); //将扫描码(现在是断码)还原成通码
    	if(make_code == ctrl_l_make || make_code == ctrl_r_make) 
            ctrl_status = 0;           //判断是否松开了ctrl
    	else if(make_code == shift_l_make || make_code == shift_r_make) 
            shift_status = 0;   //判断是否松开了shift
    	else if(make_code == alt_l_make || make_code == alt_r_make) 
            alt_status = 0;         //判断是否松开了alt
    	return;
    }
    //来到这里，说明不是断码，而是通码，这里的判断是保证我们只处理这些数组中定义了的键，以及右alt和ctrl。
    else if((scancode > 0x00 && scancode < 0x3b) || (scancode == alt_r_make) || (scancode == ctrl_r_make))
    {
    	int shift = 0; //确定是否开启shift的标志，先默认设置成0
    	uint8_t index = (scancode & 0x00ff);    //将扫描码留下低字节，这就是在数组中对应的索引
    
	    if(scancode == ctrl_l_make || scancode == ctrl_r_make)  //如果扫描码是ctrl_l_make，或者ctrl_r_make，说明按下了ctrl    	
	    {
            ctrl_status = 1;
            return;
        }
	    else if(scancode == shift_l_make || scancode == shift_r_make)
        {
            shift_status = 1;
            return;
        }
	    else if(scancode == alt_l_make || scancode == alt_r_make)
        {
            alt_status = 1;
            return;
        }
	    else if(scancode == caps_lock_make) //大写锁定键是按一次，然后取反
        {
            caps_lock_status = !caps_lock_status;
            return;
        }

		if ((scancode < 0x0e) || (scancode == 0x29) || (scancode == 0x1a) || \
			(scancode == 0x1b) || (scancode == 0x2b) || (scancode == 0x27) || \
			(scancode == 0x28) || (scancode == 0x33) || (scancode == 0x34) || (scancode == 0x35)) {  
			/*代表两个字母的键 0x0e 数字'0'~'9',字符'-',字符'='
                           0x29 字符'`'
                           0x1a 字符'['
                           0x1b 字符']'
                           0x2b 字符'\\'
                           0x27 字符';'
                           0x28 字符'\''
                           0x33 字符','
                           0x34 字符'.'
                           0x35 字符'/' 
            */
         	if (shift_status)// 如果同时按下了shift键
            	shift = true;
      	} 
      	else {	  // 默认为字母键
			if(shift_status + caps_lock_status == 1)
            	shift = 1;   //shift和大写锁定，那么判断是否按下了一个，而且不能是同时按下，那么就能确定是要开启shift
      	}
        char cur_char = keymap[index][shift];
        if (cur_char) {
            /*****************  快捷键ctrl+l和ctrl+u的处理 *********************
             * 下面是把ctrl+l和ctrl+u这两种组合键产生的字符置为:
             * cur_char的asc码-字符a的asc码, 此差值比较小,
             * 属于asc码表中不可见的字符部分.故不会产生可见字符.
             * 我们在shell中将ascii值为l-a和u-a的分别处理为清屏和删除输入的快捷键*/
	        if ((ctrl_status && cur_char == 'l') || (ctrl_status && cur_char == 'u')) {
	            cur_char -= 'a';
	        }
            if (!ioq_full(&kbd_buf)) {
                put_char(cur_char);	    // 临时的
                ioq_putchar(&kbd_buf, cur_char);
            }
	        return;
        }
    }
    else 
		put_str("unknown key\n");
    return;
}

/* 键盘初始化 */
void keyboard_init() {
   put_str("keyboard init start\n");
   ioqueue_init(&kbd_buf);
   register_handler(0x21, intr_keyboard_handler);
   put_str("keyboard init done\n");
}

