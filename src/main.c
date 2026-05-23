#include "mcs51/8052.h"
#include "../include/Delay.h"
#include "../include/LCD1602.h"
#include "../include/MatrixKey.h"

// --- 全局变量定义 ---
int KeyValue = 0;
static unsigned int TimeCount = 0;

// 时间数据
static unsigned int TimeSec = 0;
static unsigned int TimeMinute = 0;
static unsigned int RealHour = 12;     // 底层真实时间 (24小时制 0-23)，这是核心数据
static unsigned int TimeHour = 12;      // 显示用的时间 (随 12/24 制变化)
static unsigned char TimeHour_flag = 0; // 0: 24小时制, 1: 12小时制

static unsigned int TimeDay = 30;
static unsigned int TimeMonth = 5;
static unsigned int TimeYear = 2026;

// 设置用的临时变量
static unsigned int AlterSec = 0;
static unsigned int AlterMinute = 59;
static unsigned int AlterRealHour = 12; // 对应 RealHour
static unsigned int AlterDay = 30;
static unsigned int AlterMonth = 5;
static unsigned int AlterYear = 2026;

static unsigned int AlterFlag = 1; // 1:Sec, 2:Min, 3:Hour, 4:Day, 5:Month, 6:Year
static unsigned char Menu_flag = 0; // 0:主界面, 1:设置界面

// --- 函数声明 ---
void buzzer(int state);
void Timer0Init(void);
void Calendar(void);
void Main_ShowData(void);
unsigned char GetDaysInMonth(unsigned int year, unsigned char month);
void KeyLogic(int key);
void AlterTime_ShowData(void);
void SwopAlter(void);
void AlterSwop(void);
void SetAlarm_ShowData(void);
void BlinkField(unsigned char row, unsigned char col, unsigned int value, unsigned char digits);

// --- 中断函数声明 ---
static void Timer0_Rountine(void) __interrupt (1);

// --- 主函数 ---
int main(void) {
    buzzer(0);
    LCD_Init();
    Timer0Init();

    while(1) {
        KeyValue = MatrixKey();

        switch (Menu_flag) {
            case 0: // 主界面
                Main_ShowData();
                Calendar();   // 只有主界面才进行时间进位计算
                break;
            case 1: // 设置界面
                AlterTime_ShowData();
                break;
            case 2: // 闹钟设置界面
                SetAlarm_ShowData();
                break;
        }

        KeyLogic(KeyValue);
    }
}

// --- 数据同步函数 ---
// 进入设置界面时，把真实时间复制给 Alter 变量
void SwopAlter(void) {
    AlterSec = TimeSec;
    AlterMinute = TimeMinute;
    AlterRealHour = RealHour; // 同步底层24小时制数据
    AlterDay = TimeDay;
    AlterMonth = TimeMonth;
    AlterYear = TimeYear;
}

// 退出设置界面时，把 Alter 变量写回真实时间
void AlterSwop(void) {
    TimeSec = AlterSec;
    TimeMinute = AlterMinute;
    RealHour = AlterRealHour; // 写回底层24小时制数据
    TimeDay = AlterDay;
    TimeMonth = AlterMonth;
    TimeYear = AlterYear;
}

// --- 显示函数 ---
void AlterTime_ShowData(void) {
    LCD_ShowString(2, 1, "Alter");

    // 1. 计算显示用的小时 (处理 12/24 小时制转换)
    unsigned int DisplayHour;
    if (TimeHour_flag == 0) {
        DisplayHour = AlterRealHour; // 24小时制直接显示
    } else {
        if (AlterRealHour == 0) DisplayHour = 12;      // 0点 -> 12 AM
        else if (AlterRealHour > 12) DisplayHour = AlterRealHour - 12; // 13-23 -> 1-11
        else DisplayHour = AlterRealHour;             // 1-12 -> 1-12
    }

    // 2. 闪烁逻辑 (根据 AlterFlag)
    switch (AlterFlag) {
        case 1: BlinkField(2, 15, AlterSec, 2); break;
        case 2: BlinkField(2, 12, AlterMinute, 2); break;
        case 3: BlinkField(2, 9, DisplayHour, 2); break; // 显示转换后的小时
        case 4: BlinkField(1, 9, AlterDay, 2); break;
        case 5: BlinkField(1, 6, AlterMonth, 2); break;
        case 6: BlinkField(1, 1, AlterYear, 4); break;
    }

    // 3. 非闪烁字段的稳定显示
    if (AlterFlag != 1) LCD_ShowNum(2, 15, AlterSec, 2);
    if (AlterFlag != 2) LCD_ShowNum(2, 12, AlterMinute, 2);
    if (AlterFlag != 3) LCD_ShowNum(2, 9, DisplayHour, 2); // 注意这里
    if (AlterFlag != 4) LCD_ShowNum(1, 9, AlterDay, 2);
    if (AlterFlag != 5) LCD_ShowNum(1, 6, AlterMonth, 2);
    if (AlterFlag != 6) LCD_ShowNum(1, 1, AlterYear, 4);

    // 4. 固定分隔符
    LCD_ShowChar(2, 14, ':');
    LCD_ShowChar(2, 11, ':');
    LCD_ShowChar(1, 8, '-');
    LCD_ShowChar(1, 5, '-');
}

void SetAlarm_ShowData(void)
{
    unsigned int DisplayHour;
    if (TimeHour_flag == 0) {
        DisplayHour = RealHour;
    } else {
        if (RealHour == 0) DisplayHour = 12;
        else if (RealHour > 12) DisplayHour = RealHour - 12;
        else DisplayHour = RealHour;
    }

    LCD_ShowString(1, 5, "SetAlarm");;
    LCD_ShowNum(2, 11, TimeSec, 2);
    LCD_ShowChar(2, 10, ':');
    LCD_ShowNum(2, 8, TimeMinute, 2);
    LCD_ShowChar(2, 7, ':');
    LCD_ShowNum(2, 5, DisplayHour, 2);
}

void Main_ShowData(void) {
    LCD_ShowString(2, 1, "Main");

    // 主界面同样需要根据 flag 计算显示小时
    unsigned int DisplayHour;
    if (TimeHour_flag == 0) {
        DisplayHour = RealHour;
    } else {
        if (RealHour == 0) DisplayHour = 12;
        else if (RealHour > 12) DisplayHour = RealHour - 12;
        else DisplayHour = RealHour;
    }

    LCD_ShowNum(2, 15, TimeSec, 2);
    LCD_ShowChar(2, 14, ':');
    LCD_ShowNum(2, 12, TimeMinute, 2);
    LCD_ShowChar(2, 11, ':');
    LCD_ShowNum(2, 9, DisplayHour, 2);
    LCD_ShowNum(1, 9, TimeDay, 2);
    LCD_ShowChar(1, 8, '-');
    LCD_ShowNum(1, 6, TimeMonth, 2);
    LCD_ShowChar(1, 5, '-');
    LCD_ShowNum(1, 1, TimeYear, 4);
}

// 通用闪烁函数
void BlinkField(unsigned char row, unsigned char col, unsigned int value, unsigned char digits) {
    static unsigned char blink_state = 0;
    if (TimeCount % 4 == 0) {
        blink_state = !blink_state;
        if (blink_state) {
            LCD_ShowNum(row, col, value, digits);
        } else {
            char spaces[5] = "    ";
            spaces[digits] = '\0';
            LCD_ShowString(row, col, spaces);
        }
    }
}

// --- 时间核心逻辑 ---
void Calendar(void) {
    // 秒进位
    if (TimeSec >= 60) {
        TimeSec = 0;
        TimeMinute++;
    }
    // 分进位
    if (TimeMinute >= 60) {
        TimeMinute = 0;
        RealHour++; // 操作底层24小时制
    }
    // 时进位 (底层逻辑永远是 0-23)
    if (RealHour >= 24) {
        RealHour = 0;
        TimeDay++;
    }

    // 日期进位逻辑 (月份/年份)
    unsigned char maxDay = GetDaysInMonth(TimeYear, TimeMonth);
    if (TimeDay > maxDay) {
        TimeDay = 1;
        TimeMonth++;
        if (TimeMonth > 12) {
            TimeMonth = 1;
            TimeYear++;
        }
    }
}

// 获取月份天数
unsigned char GetDaysInMonth(unsigned int year, unsigned char month) {
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29;
        else return 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    } else {
        return 31;
    }
}

// --- 按键逻辑 ---
void KeyLogic(int key) {
    switch (key) {
        // 确认键 (切换界面)
        case 1:
            if (Menu_flag == 0) {
                SwopAlter();     // 真实 -> Alter
                Menu_flag = 1;   // 进入设置
            } else {
                AlterSwop();     // Alter -> 真实
                Menu_flag = 0;   // 返回主界面
            }
            break;
        case 2:
            if (Menu_flag == 0) {
                LCD_Clear();
                Menu_flag = 2; // 进入闹钟设置菜单
            } else {
                LCD_Clear();
                AlterSwop();
                Menu_flag = 0; // 返回主界面
            }
            break;
        // 切换时制键
        case 3:
            // 直接切换标志位
            TimeHour_flag = !TimeHour_flag;
            // 注意：不需要调用 Calendar()，也不需要修改 RealHour
            // 下次显示时，会自动根据新的 flag 进行转换
            break;

        // 加键
        case 4:
            switch (AlterFlag) {
                case 1: if (++AlterSec > 59) AlterSec = 0; break;
                case 2: if (++AlterMinute > 59) AlterMinute = 0; break;

                // --- 修改 AlterRealHour，保持底层数据纯净 ---
                case 3: if (++AlterRealHour > 23) AlterRealHour = 0; break;

                case 4:
                    if (++AlterDay > GetDaysInMonth(AlterYear, AlterMonth)) {
                        AlterDay = 1;
                    }
                    break;

                case 5:
                    if (++AlterMonth > 12) AlterMonth = 1;

                    // --- 核心修复：防止 31号 加到 4月/6月 导致死循环 ---
                    unsigned char maxDay = GetDaysInMonth(AlterYear, AlterMonth);
                    if (AlterDay > maxDay) {
                        AlterDay = maxDay; // 自动修正为该月最大天数
                    }
                    break;

                case 6: if (++AlterYear > 9999) AlterYear = 1900; break;
            }
            break;

        // 减键 (逻辑同上，注意 AlterRealHour 和 日期修正)
        case 7:
            switch (AlterFlag) {
                case 1: if (AlterSec-- == 0) AlterSec = 59; break;
                case 2: if (AlterMinute-- == 0) AlterMinute = 59; break;
                case 3:
                    if (AlterRealHour-- == 0) AlterRealHour = 23;
                    break;
                case 4:
                    if (AlterDay-- == 1) AlterDay = GetDaysInMonth(AlterYear, AlterMonth);
                    break;
                case 5:
                    if (AlterMonth == 1) AlterMonth = 12;
                    else AlterMonth--;

                    // --- 同样进行日期修正 ---
                    unsigned char maxDay = GetDaysInMonth(AlterYear, AlterMonth);
                    if (AlterDay > maxDay) {
                        AlterDay = maxDay;
                    }
                    break;
                case 6: if (AlterYear-- == 1900) AlterYear = 9999; break;
            }
            break;

        // 光标移动
        case 5: AlterFlag++; if (AlterFlag > 6) AlterFlag = 1; break;
        case 6: AlterFlag--; if (AlterFlag < 1) AlterFlag = 6; break;
    }
    KeyValue = 0;
}

// --- 硬件初始化与中断 ---
void Timer0Init(void) {
    TMOD &= 0xF0;
    TMOD |= 0x01;
    TH0 = 64535 / 256;
    TL0 = 64535 % 256 + 1;
    TF0 = 0;
    TR0 = 1;
    ET0 = 1;
    EA = 1;
}

void Timer0_Rountine(void) __interrupt (1) {
    TH0 = 64535 / 256;
    TL0 = 64535 % 256 + 1;
    TimeCount++;
    if(TimeCount >= 100) { // 假设10ms中断, 100次=1s
        TimeSec++;
        TimeCount = 0;
    }
}

void buzzer(int state) {
    if(state) P2_3 = 1;
    else P2_3 = 0;
}