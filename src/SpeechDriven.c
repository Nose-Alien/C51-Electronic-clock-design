#include <stdint.h>
#include "../include/SpeechDriven.h"
#include "mcs51/8052.h"

void InitUART(void) //9600bps@11.0592MHz
{
    PCON &= 0x7F; //波特率不倍速
    SCON = 0x40; //8位数据,可变波特率
    TMOD &= 0x0F; //清除定时器1模式位
    TMOD |= 0x20; //设定定时器1为8位自动重装方式
    TL1 = 0xFD; //设定定时初值
    TH1 = 0xFD; //设定定时器重装值
    TR1 = 1; //启动定时器1
    ET1 = 0; //禁止定时器1中断
}

void TelTime(unsigned int Hour_flag,unsigned int hour, unsigned int minute, unsigned int sec)//整点报时
{
    if (minute == 0 && sec == 0 && (hour >=8 && hour <= 22)&&Hour_flag==0) {
        SendBeijingTime(hour);
    }else if (minute == 0 && sec == 0 && (hour >=8 && hour <= 22)&&Hour_flag==1) {
        SendBeijingTime_12H(hour);
    }
}


void SendBeijingTime(unsigned int hour)
{
    // 1. 发送 [
    Uartsend('[');

    // 2. 发送 "现在是北京时间" (7个汉字 → 14字节 GBK)
    Uartsend(0xCF);
    Uartsend(0xD6); // 现
    Uartsend(0xD4);
    Uartsend(0xDA); // 在
    Uartsend(0xCA);
    Uartsend(0xC7); // 是
    Uartsend(0xB1);
    Uartsend(0xB1); // 北
    Uartsend(0xBE);
    Uartsend(0xA9); // 京
    Uartsend(0xCA);
    Uartsend(0xB1); // 时
    Uartsend(0xBC);
    Uartsend(0xE4); // 间

    // 3. 动态发送小时数字 (0-23)
    if (hour >= 10) {
        Uartsend('0' + (hour / 10)); // 十位
        Uartsend('0' + (hour % 10)); // 个位
    } else {
        Uartsend('0' + hour); // 个位
    }

    // 4. 发送 "点整" (2个汉字 → 4字节 GBK)
    Uartsend(0xB5);
    Uartsend(0xE3); // 点
    Uartsend(0xD5);
    Uartsend(0xFB); // 整

    // 5. 发送 ]
    Uartsend(']');
}

/**
 * 发送12小时制北京时间 (上午/下午 x点整)
 * @param hour 24小时制小时 (0-23)
 * 示例: hour=14 → [现在是北京时间下午2点整]
 */
void SendBeijingTime_12H(unsigned int hour) {
    // 1. 发送 [
    Uartsend('[');

    // 2. 发送 "现在是北京时间" (14字节)
    Uartsend(0xCF); Uartsend(0xD6); // 现
    Uartsend(0xD4); Uartsend(0xDA); // 在
    Uartsend(0xCA); Uartsend(0xC7); // 是
    Uartsend(0xB1); Uartsend(0xB1); // 北
    Uartsend(0xBE); Uartsend(0xA9); // 京
    Uartsend(0xCA); Uartsend(0xB1); // 时
    Uartsend(0xBC); Uartsend(0xE4); // 间

    // 3. 计算12小时制并发送时段标识
    unsigned int display_hour;
    if (hour == 0 || hour == 12) {
        display_hour = 12;
    } else {
        display_hour = hour % 12;
    }

    if (hour < 12) { // 上午 (0-11)
        Uartsend(0xC9); Uartsend(0xCF); // 上
        Uartsend(0xCE); Uartsend(0xE7); // 午
    } else {          // 下午 (12-23)
        Uartsend(0xCF); Uartsend(0xC2); // 下
        Uartsend(0xCE); Uartsend(0xE7); // 午
    }

    // 4. 发送小时数字 (1-12，无前导零)
    if (display_hour >= 10) { // 10-12点
        Uartsend('0' + (display_hour / 10));
        Uartsend('0' + (display_hour % 10));
    } else {                 // 1-9点
        Uartsend('0' + display_hour);
    }

    // 5. 发送 "点整" (4字节)
    Uartsend(0xB5); Uartsend(0xE3); // 点
    Uartsend(0xD5); Uartsend(0xFB); // 整

    // 6. 发送 ]
    Uartsend(']');
}

void Uartsend(unsigned char byte) //发送
{
    SBUF = byte; //把数据写入发送缓冲区SBUF
    //数据发送完成的标志是TI=1；所以等待数据传送完
    while (TI == 0);
    TI = 0; //软件清零
}

void UART_Routine(void)__interrupt(4)
{
    if (RI) {
        RI = 0;
    } else
        TI = 0;
}
