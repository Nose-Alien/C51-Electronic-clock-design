#include "mcs51/8052.h"
#include "../include/Delay.h"
#include "../include/LCD1602.h"
#include "../include/MatrixKey.h"

int KeyValue = 0;
static unsigned int TimeCount = 0;
static unsigned int TimeSec = 0;
static unsigned int TimeMinute = 59;
static unsigned int RealHour = 23;      // 底层真实时间（24小时制）
static unsigned int TimeHour = 23;      // 显示用的时间（随标志位变化）
static unsigned int TimeDay = 31;
static unsigned int TimeMonth = 5;
static unsigned int TimeYear = 2026;
static unsigned char TimeHour_flag = 0; // 0: 24小时制, 1: 12小时制

// 函数声明（注意：中断函数在 main 后定义，必须加 static 声明）
void buzzer(int state);
void test(void);
void Timer0Init(void);
void Calendar(void);
void UP_ShowData(void);
unsigned char GetDaysInMonth(unsigned int year, unsigned char month);
void KeyLogic(int key);
static void Timer0_Rountine(void) __interrupt (1); // 加上 static

int main(void) {
    buzzer(0);
    LCD_Init();
    Timer0Init();
    while(1) {
        KeyValue=MatrixKey();
        UP_ShowData();
        Calendar();
        KeyLogic(KeyValue);
    }
}
void buzzer(int state)
{
    if(state){
        P2_3 = 1;
    } else {
        P2_3 = 0;
    }
}

void Calendar(void) {
    // 秒进位
    if (TimeSec >= 60) {
        TimeSec = 0;
        TimeMinute++;
    }
    // 分进位
    if (TimeMinute >= 60) {
        TimeMinute = 0;
        RealHour++; // 真实时间累加
    }

    // --- 真实时间（底层）的时进位到日 ---
    if (RealHour >= 24) {
        RealHour = 0;
        TimeDay++;
    }

    // --- 同步显示时间 TimeHour ---
    if (TimeHour_flag == 0) {
        // 24小时制：直接显示真实时间
        TimeHour = RealHour;
    } else {
        // 12小时制：转换显示
        if (RealHour == 0) {
            TimeHour = 12;      // 凌晨0点显示为12
        } else if (RealHour > 12) {
            TimeHour = RealHour - 12;
        } else {
            TimeHour = RealHour;
        }
    }

    // --- 日进位到月 ---
    if (TimeDay > GetDaysInMonth(TimeYear, TimeMonth)) {
        TimeDay = 1;
        TimeMonth++;
    }
    // --- 月进位到年 ---
    if (TimeMonth > 12) {
        TimeMonth = 1;
        TimeYear++;
    }
}

unsigned char GetDaysInMonth(unsigned int year, unsigned char month) {
    if (month == 1 || month == 3 || month == 5 || month == 7 ||
        month == 8 || month == 10 || month == 12) {
        return 31;
        } else if (month == 4 || month == 6 || month == 9 || month == 11) {
            return 30;
        } else if (month == 2) {
            // 判断闰年：能被4整除且不能被100整除，或者能被400整除
            if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
                return 29; // 闰年2月
            } else {
                return 28; // 平年2月
            }
        }
    return 30; // 默认兜底
}

void UP_ShowData(void)
{
    LCD_ShowNum(2,15,TimeSec,2);
    LCD_ShowChar(2,14,'-');
    LCD_ShowNum(2,12,TimeMinute,2);
    LCD_ShowChar(2,11,'-');
    LCD_ShowNum(2,9,TimeHour,2);

    LCD_ShowNum(1,9,TimeDay,2);
    LCD_ShowChar(1,8,'-');
    LCD_ShowNum(1,6,TimeMonth,2);
    LCD_ShowChar(1,5,'-');
    LCD_ShowNum(1, 1, TimeYear, 4);
}

void KeyLogic(int key)
{

    switch (key) {
        P1_7=0;
        case 1:
            LCD_ShowNum(1, 1, 1, 1);
            break;
        case 2:
            LCD_ShowNum(1, 1, 2, 1);
            break;
        case 3:
            if (TimeHour_flag == 0) {
                TimeHour_flag = 1; // 如果当前是24小时制，切换为12小时制
            } else {
                TimeHour_flag = 0; // 如果当前是12小时制，切换为24小时制
            }
            Calendar(); // 立即刷新一下 TimeHour 的显示值
            UP_ShowData();
            break;
        case 4:
            LCD_ShowNum(1, 1, 4, 1);
            break;
        case 5:
            LCD_ShowNum(1, 1, 5, 1);
            break;
        case 6:
            LCD_ShowNum(1, 1, 6, 1);
            break;
        case 7:
            LCD_ShowNum(1, 1, 7, 1);
            break;
        default:
            break;
    }
    KeyValue = 0;
}


void test(void)
{
    // key=MatrixKey();
    switch (KeyValue) {
        case 1:
            LCD_ShowNum(1, 1, 1, 1);
            break;
        case 2:
            LCD_ShowNum(1, 1, 2, 1);
            break;
        case 3:
            LCD_ShowNum(1, 1, 3, 1);
            break;
        case 4:
            LCD_ShowNum(1, 1, 4, 1);
            break;
        case 5:
            LCD_ShowNum(1, 1, 5, 1);
            break;
        case 6:
            LCD_ShowNum(1, 1, 6, 1);
            break;
        case 7:
            LCD_ShowNum(1, 1, 7, 1);
            break;
        default:
            break;
    }
    // P1 = 0x00;
    // buzzer(1);
    // Delay(1000);
    // P1 = 0xFF;
    // buzzer(0);
    // Delay(1000);
}

void Timer0Init(void)
{
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 64535 / 256;
    TL0 = 64535 % 256 + 1;
    TF0 = 0;
    TR0 = 1;
    ET0 = 1;
    EA = 1;
    PT0 = 0;
}

void Timer0_Rountine(void) __interrupt (1)
{
    TH0 = 64535 / 256;           // 高8位
    TL0 = 64535 % 256 + 1;       // 低8位
    TimeCount++;
    if(TimeCount % 5 == 0)
    {
        // KeyValue=MatrixKey();
    }
    if(TimeCount >= 100)
    {
        TimeSec++;
        TimeCount = 0;
    }
}