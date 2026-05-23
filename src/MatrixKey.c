#include "mcs51/8052.h"
#include "../include/Delay.h"

/**
  * @brief  矩阵键盘读取按键键码
  * @param  无
  * @retval KeyNumber 按下按键的键码值
			如果按键按下不放，程序会停留在此函数，松手的一瞬间，返回按键键码，没有按键按下时，返回0
  */
unsigned char MatrixKey(void)
{
	unsigned char KeyNumber=0;

	if(P1_0==0){Delay(20);while(P1_0==0);Delay(20);KeyNumber=1;}
	if(P1_1==0){Delay(20);while(P1_1==0);Delay(20);KeyNumber=2;}
	if(P1_2==0){Delay(20);while(P1_2==0);Delay(20);KeyNumber=3;}
	if(P1_3==0){Delay(20);while(P1_3==0);Delay(20);KeyNumber=4;}
	if(P1_4==0){Delay(20);while(P1_4==0);Delay(20);KeyNumber=5;}
	if(P1_5==0){Delay(20);while(P1_5==0);Delay(20);KeyNumber=6;}
	if(P1_6==0){Delay(20);while(P1_6==0);Delay(20);KeyNumber=7;}
	// if(P1_7==0){Delay(20);while(P1_7==0);Delay(20);KeyNumber=8;}

	return KeyNumber;
}
