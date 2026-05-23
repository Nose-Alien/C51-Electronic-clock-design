#include "mcs51/8052.h"
#include "../include/Delay.h"
#include "../include/LCD1602.h"
#include "../include/MatrixKey.h"

// --- 全局变量定义 ---
int KeyValue = 0;
static unsigned int TimeCount = 0;

// 时间数据 (核心)
static unsigned int TimeSec = 0;
static unsigned int TimeMinute = 0;
static unsigned int RealHour = 12;     // 底层真实时间 (24小时制 0-23)
static unsigned int TimeHour = 12;     // 显示用的时间 (随 12/24 制变化)
static unsigned char TimeHour_flag = 0; // 0: 24小时制, 1: 12小时制
static unsigned int TimeDay = 30;
static unsigned int TimeMonth = 5;
static unsigned int TimeYear = 2026;

// 闹钟数据
static unsigned char AlarmOnOff = 1;   // 闹钟开关(0关，1开)
static unsigned int AlarmHour = 12;    // 闹钟时 (24h制)
static unsigned int AlarmMinute = 1;   // 闹钟分
static unsigned char AlarmFlag = 3;   // 闹钟光标标志位 (1=开关, 2=分, 3=时)

// 闹钟触发状态
static unsigned char AlarmTriggered = 0; // 1表示闹钟正在响

// 设置用的临时变量
static unsigned int AlterSec = 0;
static unsigned int AlterMinute = 59;
static unsigned int AlterRealHour = 12; // 对应 RealHour
static unsigned int AlterDay = 30;
static unsigned int AlterMonth = 5;
static unsigned int AlterYear = 2026;
static unsigned int AlterFlag = 1;      // 1:Sec, 2:Min, 3:Hour, 4:Day, 5:Month, 6:Year

// 菜单界面标志
static unsigned char Menu_flag = 0;    // 0:主界面, 1:设置界面, 2:闹钟界面

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
void AlarmState(void);

// --- 中断函数声明 ---
static void Timer0_Rountine(void) __interrupt (1);

/**
 * @brief 主函数
 * 初始化硬件并进入主循环，处理按键、闹钟状态和界面显示
 */
int main(void) {
    buzzer(0);
    LCD_Init();
    Timer0Init();

    while(1) {
        // 1. 读取按键值
        KeyValue = GetKey();

        // 2. 检查闹钟是否触发
        AlarmState();

        // 3. 处理按键逻辑
        KeyLogic(KeyValue);

        // 4. 根据当前菜单标志显示不同界面
        switch (Menu_flag) {
            case 0: // 主界面
                Main_ShowData();
                Calendar(); // 只有主界面才进行时间进位计算
                break;
            case 1: // 设置界面
                AlterTime_ShowData();
                break;
            case 2: // 闹钟设置界面
                SetAlarm_ShowData();
                break;
        }
    }
}

/**
 * @brief 闹钟状态检查函数
 * 检查当前时间是否等于闹钟时间，并控制蜂鸣器
 */
void AlarmState(void) {
    if (AlarmOnOff) {
        // 如果闹钟开启且时间匹配
        if (RealHour == AlarmHour && TimeMinute == AlarmMinute) {
            AlarmTriggered = 1; // 设置触发标志
            buzzer(1);          // 开启蜂鸣器
        }
    } else {
        buzzer(0); // 闹钟关闭则关闭蜂鸣器
    }
}

/**
 * @brief 进入设置界面时的数据同步
 * 将真实运行的时间数据复制到修改用的临时变量中
 */
void SwopAlter(void) {
    AlterSec = TimeSec;
    AlterMinute = TimeMinute;
    AlterRealHour = RealHour;
    AlterDay = TimeDay;
    AlterMonth = TimeMonth;
    AlterYear = TimeYear;
}

/**
 * @brief 退出设置界面时的数据写回
 * 将修改后的临时变量写回真实运行的时间数据
 */
void AlterSwop(void) {
    TimeSec = AlterSec;
    TimeMinute = AlterMinute;
    RealHour = AlterRealHour;
    TimeDay = AlterDay;
    TimeMonth = AlterMonth;
    TimeYear = AlterYear;
}

/**
 * @brief 设置界面显示函数
 * 显示可编辑的时间日期，并根据光标位置闪烁
 */
void AlterTime_ShowData(void) {
    LCD_ShowString(2, 1, "Alter");

    // 计算显示用的小时（处理12/24小时制转换）
    unsigned int DisplayHour;
    if (TimeHour_flag == 0) {
        DisplayHour = AlterRealHour;
    } else {
        if (AlterRealHour == 0) DisplayHour = 12;
        else if (AlterRealHour > 12) DisplayHour = AlterRealHour - 12;
        else DisplayHour = AlterRealHour;
    }

    // 根据 AlterFlag 控制闪烁
    switch (AlterFlag) {
        case 1: BlinkField(2, 15, AlterSec, 2); break;
        case 2: BlinkField(2, 12, AlterMinute, 2); break;
        case 3: BlinkField(2, 9, DisplayHour, 2); break;
        case 4: BlinkField(1, 9, AlterDay, 2); break;
        case 5: BlinkField(1, 6, AlterMonth, 2); break;
        case 6: BlinkField(1, 1, AlterYear, 4); break;
    }

    // 显示非闪烁部分的固定数据
    if (AlterFlag != 1) LCD_ShowNum(2, 15, AlterSec, 2);
    if (AlterFlag != 2) LCD_ShowNum(2, 12, AlterMinute, 2);
    if (AlterFlag != 3) LCD_ShowNum(2, 9, DisplayHour, 2);
    if (AlterFlag != 4) LCD_ShowNum(1, 9, AlterDay, 2);
    if (AlterFlag != 5) LCD_ShowNum(1, 6, AlterMonth, 2);
    if (AlterFlag != 6) LCD_ShowNum(1, 1, AlterYear, 4);

    // 显示固定分隔符
    LCD_ShowChar(2, 14, ':');
    LCD_ShowChar(2, 11, ':');
    LCD_ShowChar(1, 8, '-');
    LCD_ShowChar(1, 5, '-');
}

/**
 * @brief 闹钟设置界面显示函数
 * 显示闹钟设置项，并根据光标位置闪烁
 */
void SetAlarm_ShowData(void) {
    unsigned int DisplayHour;

    // 计算闹钟显示小时（处理12/24小时制转换）
    if (TimeHour_flag == 0) {
        DisplayHour = AlarmHour;
    } else {
        if (AlarmHour == 0) DisplayHour = 12;
        else if (AlarmHour > 12) DisplayHour = AlarmHour - 12;
        else DisplayHour = AlarmHour;
    }

    // 显示固定标题
    LCD_ShowString(1, 5, "SetAlarm");
    LCD_ShowChar(2, 6, ':');

    // 根据 AlarmFlag 控制闪烁逻辑
    if (AlarmFlag == 3) {
        // 小时闪烁
        BlinkField(2, 4, DisplayHour, 2);
        LCD_ShowNum(2, 7, AlarmMinute, 2);
        LCD_ShowString(2, 11, AlarmOnOff ? "ON " : "OFF");
    } else if (AlarmFlag == 2) {
        // 分钟闪烁
        BlinkField(2, 7, AlarmMinute, 2);
        LCD_ShowNum(2, 4, DisplayHour, 2);
        LCD_ShowString(2, 11, AlarmOnOff ? "ON " : "OFF");
    } else if (AlarmFlag == 1) {
        // 开关闪烁
        LCD_ShowNum(2, 7, AlarmMinute, 2);
        LCD_ShowNum(2, 4, DisplayHour, 2);
        // 简单的闪烁效果
        if (AlarmOnOff) {
            LCD_ShowString(2, 11, "ON ");
        } else {
            LCD_ShowString(2, 11, "OFF");
        }
    }
}

/**
 * @brief 主界面显示函数
 * 显示实时运行的时间和日期
 */
void Main_ShowData(void) {
    LCD_ShowString(2, 1, "Main");

    // 计算显示小时
    unsigned int DisplayHour;
    if (TimeHour_flag == 0) {
        DisplayHour = RealHour;
    } else {
        if (RealHour == 0) DisplayHour = 12;
        else if (RealHour > 12) DisplayHour = RealHour - 12;
        else DisplayHour = RealHour;
    }

    // 显示时间日期数据
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

/**
 * @brief 通用闪烁函数
 * 控制光标所在字段的闪烁显示
 */
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

/**
 * @brief 时间进位计算 (万年历核心逻辑)
 * 处理秒、分、时、日的进位及闰年判断
 */
void Calendar(void) {
    // 秒进位
    if (TimeSec >= 60) {
        TimeSec = 0;
        TimeMinute++;
    }
    // 分进位
    if (TimeMinute >= 60) {
        TimeMinute = 0;
        RealHour++;
    }
    // 时进位 (底层逻辑永远是 0-23)
    if (RealHour >= 24) {
        RealHour = 0;
        TimeDay++;
    }
    // 日期进位逻辑
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

/**
 * @brief 获取月份天数
 * 根据年份和月份计算当月最大天数（含闰年判断）
 */
unsigned char GetDaysInMonth(unsigned int year, unsigned char month) {
    if (month == 2) {
        // 闰年判断
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
            return 29;
        else
            return 28;
    } else if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    } else {
        return 31;
    }
}

/**
 * @brief 按键逻辑处理函数
 * 根据 KeyValue 和当前界面执行相应操作
 */
void KeyLogic(int key) {
    switch (key) {
        // 确认键 (切换界面)
        case 1:
            if (Menu_flag == 0) {
                LCD_Clear();
                SwopAlter();     // 真实 -> Alter
                Menu_flag = 1;   // 进入设置
            } else {
                LCD_Clear();
                AlterSwop();     // Alter -> 真实
                Menu_flag = 0;   // 返回主界面
            }
            break;

        // 闹钟/返回键
        case 2:
            if (AlarmTriggered == 0) {
                // 未响铃时切换界面
                if (Menu_flag == 0) {
                    LCD_Clear();
                    Menu_flag = 2;
                } else {
                    LCD_Clear();
                    Menu_flag = 0;
                }
            } else {
                // 闹钟响铃时关闭闹钟
                AlarmTriggered = 0;
                buzzer(0);
            }
            break;

        // 切换时制键 (12/24)
        case 3:
            TimeHour_flag = !TimeHour_flag;
            break;

        // 加键
        case 4:
            if (Menu_flag == 1) {
                // 设置界面加操作
                switch (AlterFlag) {
                    case 1: if (++AlterSec > 59) AlterSec = 0; break;
                    case 2: if (++AlterMinute > 59) AlterMinute = 0; break;
                    case 3: if (++AlterRealHour > 23) AlterRealHour = 0; break;
                    case 4:
                        if (++AlterDay > GetDaysInMonth(AlterYear, AlterMonth))
                            AlterDay = 1;
                        break;
                    case 5:
                        if (++AlterMonth > 12) AlterMonth = 1;
                        // 修正日期防止非法值
                        unsigned char maxDay = GetDaysInMonth(AlterYear, AlterMonth);
                        if (AlterDay > maxDay) AlterDay = maxDay;
                        break;
                    case 6: if (++AlterYear > 9999) AlterYear = 1900; break;
                }
            } else if (Menu_flag == 2) {
                // 闹钟界面加操作
                switch (AlarmFlag) {
                    case 2: if (++AlarmMinute > 59) AlarmMinute = 0; break;
                    case 3: if (++AlarmHour > 23) AlarmHour = 0; break;
                    case 1: AlarmOnOff = !AlarmOnOff; break;
                }
            }
            break;

        // 减键
        case 7:
            if (Menu_flag == 1) {
                // 设置界面减操作
                switch (AlterFlag) {
                    case 1: if (AlterSec-- == 0) AlterSec = 59; break;
                    case 2: if (AlterMinute-- == 0) AlterMinute = 59; break;
                    case 3: if (AlterRealHour-- == 0) AlterRealHour = 23; break;
                    case 4:
                        if (AlterDay-- == 1)
                            AlterDay = GetDaysInMonth(AlterYear, AlterMonth);
                        break;
                    case 5:
                        if (AlterMonth == 1) AlterMonth = 12;
                        else AlterMonth--;
                        // 修正日期防止非法值
                        unsigned char maxDay = GetDaysInMonth(AlterYear, AlterMonth);
                        if (AlterDay > maxDay) AlterDay = maxDay;
                        break;
                    case 6: if (AlterYear-- == 1900) AlterYear = 9999; break;
                }
            } else if (Menu_flag == 2) {
                // 闹钟界面减操作
                switch (AlarmFlag) {
                    case 2:
                        if (AlarmMinute == 0) AlarmMinute = 59;
                        else AlarmMinute--;
                        break;
                    case 3:
                        if (AlarmHour == 0) AlarmHour = 23;
                        else AlarmHour--;
                        break;
                    case 1: AlarmOnOff = !AlarmOnOff; break;
                }
            }
            break;

        // 光标右移
        case 5:
            if (Menu_flag == 1) {
                AlterFlag++;
                if (AlterFlag > 6) AlterFlag = 1;
            } else if (Menu_flag == 2) {
                AlarmFlag++;
                if (AlarmFlag > 3) AlarmFlag = 1;
            }
            break;

        // 光标左移
        case 6:
            if (Menu_flag == 1) {
                AlterFlag--;
                if (AlterFlag < 1) AlterFlag = 6;
            } else if (Menu_flag == 2) {
                AlarmFlag--;
                if (AlarmFlag < 1) AlarmFlag = 3;
            }
            break;
    }
    KeyValue = 0;
}

/**
 * @brief 定时器0初始化
 * 配置定时器0为16位定时器模式
 */
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

/**
 * @brief 定时器0中断服务函数
 * 提供时间基准 (1ms中断一次)
 */
void Timer0_Rountine(void) __interrupt (1) {
    // 重装初值 (64535对应1ms @11.0592MHz)
    TH0 = 64535 / 256;
    TL0 = 64535 % 256 + 1;

    // 计数达到100次(100ms)增加1秒
    TimeCount++;
    if(TimeCount >= 100) {
        TimeSec++;
        TimeCount = 0;
    }
}

/**
 * @brief 蜂鸣器控制函数
 * @param state 1开启, 0关闭
 */
void buzzer(int state) {
    if(state) P2_3 = 1;
    else P2_3 = 0;
}