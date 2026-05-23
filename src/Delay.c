/**
  * @file   Delay.c
  * @brief  软件延时
  * @author SleetGit
  * @date   2026/4/29.
  */

#include "mcs51/8052.h"

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

