#include "mcs51/8052.h"

/* Delay.c
 * 软件阻塞延时实现。
 * Delay(xms)：阻塞近似 xms 毫秒（与编译器与晶振频率相关）。
 */

void Delay(unsigned int xms)
{
	unsigned char i, j;
	while(xms--)
	{
		i = 2;
		j = 239;
		do
		{
			while (--j);
		} while (--i);
	}
}
