#include "mcs51/8052.h"
#include "../include/Delay.h"
#include "../include/LCD1602.h"
#include "../include/MatrixKey.h"

// --- 全局变量定义 ---
int KeyValue = 0; // 存储读取到的按键值

// 系统计时与状态
static unsigned int TimeCount = 0; // 10ms计数器 (用于产生1秒基准)

// 核心时间数据 (底层逻辑)
static unsigned int TimeSec = 0; // 当前秒 (0-59)
static unsigned int TimeMinute = 0; // 当前分钟 (0-59)
static unsigned int RealHour = 12; // 底层真实时间 (24小时制 0-23)，这是核心数据
static unsigned char TimeHour_flag = 0; // 0: 24小时制, 1: 12小时制

// 日期数据
static unsigned int TimeDay = 30; // 当前日
static unsigned int TimeMonth = 5; // 当前月
static unsigned int TimeYear = 2026; // 当前年

// 闹钟数据
static unsigned char AlarmOnOff = 0; // 闹钟开关(0关，1开)
static unsigned int AlarmHour = 12; // 闹钟时 (24h制)
static unsigned int AlarmMinute = 1; // 闹钟分
static unsigned char AlarmFlag = 3; // 闹钟光标标志位 (1=开关, 2=分, 3=时)

// --- 新增：闹钟触发状态 ---
static unsigned char AlarmTriggered = 0; // 1表示闹钟正在响

// 设置界面临时变量 (Alter前缀)
static unsigned int AlterSec = 0;
static unsigned int AlterMinute = 59;
static unsigned int AlterRealHour = 12; // 对应 RealHour
static unsigned int AlterDay = 30;
static unsigned int AlterMonth = 5;
static unsigned int AlterYear = 2026;

// 界面控制标志
static unsigned int AlterFlag = 1; // 1:Sec, 2:Min, 3:Hour, 4:Day, 5:Month, 6:Year (设置时光标位置)
static unsigned char Menu_flag = 0; // 0:主界面, 1:设置界面, 2:闹钟设置界面

// --- 函数声明 ---
void buzzer(int state); // 蜂鸣器控制函数
void Timer0Init(void); // 定时器0初始化函数
void Calendar(void); // 万年历进位计算函数
void Main_ShowData(void); // 主界面时间日期显示函数
unsigned char GetDaysInMonth(unsigned int year, unsigned char month); // 获取指定月份天数函数
void KeyLogic(int key); // 按键逻辑处理函数
void AlterTime_ShowData(void); // 修改时间界面显示函数
void SwopAlter(void); // 进入设置时：将真实时间同步到临时变量
void AlterSwop(void); // 退出设置时：将修改后的临时变量写回真实时间
void SetAlarm_ShowData(void); // 闹钟设置界面显示函数
void BlinkField(unsigned char row, unsigned char col, unsigned int value, unsigned char digits); // 通用字段闪烁显示函数
void AlarmState(void); // 闹钟状态检测与触发函数

// --- 中断函数声明 ---
static void Timer0_Rountine(void) __interrupt (1);

// --- 主函数 ---
int main(void) {
    buzzer(0);        // 初始化蜂鸣器，确保上电时处于关闭状态
    LCD_Init();       // 初始化LCD1602液晶显示屏
    Timer0Init();     // 初始化定时器0，启动系统时基（用于计时和闪烁）
    while(1) {        // 进入无限主循环
        KeyValue = GetKey();  // 扫描矩阵键盘，获取当前按下的键值
        AlarmState();         // 实时检测闹钟状态（判断是否到达设定时间并触发蜂鸣器）
        KeyLogic(KeyValue);   // 将获取到的键值传入逻辑处理函数，执行相应操作
        switch (Menu_flag) {  // 根据当前的界面标志位，切换显示不同的功能界面
            case 0: // 主界面
                Main_ShowData();  // 在主界面显示当前的时间与日期
                Calendar();       // 调用万年历函数，进行时间的秒、分、时、日进位计算
                break;            // 退出当前case
            case 1: // 设置界面
                AlterTime_ShowData(); // 在设置界面显示待修改的时间数据（带光标闪烁）
                break;                // 退出当前case
            case 2: // 闹钟设置界面
                SetAlarm_ShowData();  // 在闹钟界面显示闹钟的设定数据（带光标闪烁）
                break;                // 退出当前case
        }
    }
}

void AlarmState(void)
{
    if (AlarmOnOff) {  // 判断闹钟开关是否开启（1为开启）
        // 判断当前时间（时、分）是否与设定的闹钟时间完全匹配
        if (RealHour == AlarmHour && TimeMinute == AlarmMinute) {
            AlarmTriggered = 1; // 将闹钟触发标志置为1，表示闹钟正在响
            buzzer(1); // 调用蜂鸣器函数并传入1，开启蜂鸣器报警
        }
    } else {  // 如果闹钟开关处于关闭状态（0）
        buzzer(0); // 强制关闭蜂鸣器，确保闹钟不会误响
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
    LCD_ShowString(2, 1, "Alter"); // 在LCD第2行第1列显示标题 "Alter"

    // 1. 计算显示用的小时 (处理 12/24 小时制转换)
    unsigned int DisplayHour; // 定义一个临时变量，用来存放最终要在屏幕上显示的小时数
    if (TimeHour_flag == 0) { // 如果当前是24小时制模式
        DisplayHour = AlterRealHour; // 显示的小时数直接等于底层的24小时制数据
    } else { // 如果当前是12小时制模式
        if (AlterRealHour == 0) DisplayHour = 12;      // 底层0点转换为显示12 (凌晨)
        else if (AlterRealHour > 12) DisplayHour = AlterRealHour - 12; // 13-23点减去12，转换为下午1-11点
        else DisplayHour = AlterRealHour;             // 1-12点保持不变直接显示
    }

    // 2. 闪烁逻辑 (根据 AlterFlag 光标位置决定哪一位在闪烁)
    switch (AlterFlag) {
        case 1: BlinkField(2, 15, AlterSec, 2); break; // 光标在秒位，调用闪烁函数
        case 2: BlinkField(2, 12, AlterMinute, 2); break; // 光标在分位
        case 3: BlinkField(2, 9, DisplayHour, 2); break; // 光标在时位 (显示转换后的小时)
        case 4: BlinkField(1, 9, AlterDay, 2); break; // 光标在日位
        case 5: BlinkField(1, 6, AlterMonth, 2); break; // 光标在月位
        case 6: BlinkField(1, 1, AlterYear, 4); break; // 光标在年位
    }

    // 3. 非闪烁字段的稳定显示 (只要光标不在该位置，就强制常亮显示该数字)
    if (AlterFlag != 1) LCD_ShowNum(2, 15, AlterSec, 2); // 光标不在秒位时，正常显示秒
    if (AlterFlag != 2) LCD_ShowNum(2, 12, AlterMinute, 2); // 光标不在分位时，正常显示分
    if (AlterFlag != 3) LCD_ShowNum(2, 9, DisplayHour, 2); // 光标不在时位时，正常显示时
    if (AlterFlag != 4) LCD_ShowNum(1, 9, AlterDay, 2); // 光标不在日位时，正常显示日
    if (AlterFlag != 5) LCD_ShowNum(1, 6, AlterMonth, 2); // 光标不在月位时，正常显示月
    if (AlterFlag != 6) LCD_ShowNum(1, 1, AlterYear, 4); // 光标不在年位时，正常显示年

    // 4. 固定分隔符 (刷新显示时间日期之间的冒号和横杠)
    LCD_ShowChar(2, 14, ':'); // 显示秒与分之间的冒号
    LCD_ShowChar(2, 11, ':'); // 显示分与时之间的冒号
    LCD_ShowChar(1, 8, '-');  // 显示日与月之间的横杠
    LCD_ShowChar(1, 5, '-');  // 显示月与年之间的横杠
}

void SetAlarm_ShowData(void)
{
    unsigned int DisplayHour; // 定义临时变量，用于存放转换后要在屏幕上显示的小时数

    // --- 1. 计算显示小时 (12/24小时制转换) ---
    // 注意：这里必须使用 AlarmHour (闹钟设定值)，而不是 RealHour (当前时间)
    if (TimeHour_flag == 0) { // 如果当前处于24小时制模式
        DisplayHour = AlarmHour; // 显示的小时数直接等于设定的24小时制闹钟时间
    } else { // 如果当前处于12小时制模式
        // 12小时制转换逻辑
        if (AlarmHour == 0) DisplayHour = 12;      // 设定为0点时，显示为12 (即凌晨12点)
        else if (AlarmHour > 12) DisplayHour = AlarmHour - 12; // 设定大于12点时，减去12 (即下午时间)
        else DisplayHour = AlarmHour;              // 1-12点保持不变，直接显示
    }

    // --- 2. 显示固定内容 ---
    LCD_ShowString(1, 5, "SetAlarm"); // 在LCD第1行第5列显示标题 "SetAlarm"
    LCD_ShowChar(2, 6, ':'); // 在第2行第6列显示固定的冒号分隔符

    // --- 3. 闪烁逻辑核心 ---
    // 根据 AlarmFlag 的值决定哪个字段闪烁 (1=开关, 2=分, 3=时)
    // 注意：位置参数微调为 2,5 (小时) 和 2,7 (分钟) 以对齐格式 HH:MM

    if (AlarmFlag == 3) { // 如果光标当前在“小时”位
        // 小时闪烁 (位置 2,4)
        BlinkField(2, 4, DisplayHour, 2);
        // 非闪烁部分强制显示（防止光标移走后残留，确保其他数据可见）
        LCD_ShowNum(2, 7, AlarmMinute, 2); // 正常显示分钟
        if (AlarmOnOff) { // 根据开关状态显示 ON 或 OFF
            LCD_ShowString(2, 11, "ON ");
        } else {
            LCD_ShowString(2, 11, "OFF");
        }
    }
    else if (AlarmFlag == 2) { // 如果光标当前在“分钟”位
        // 分钟闪烁 (位置 2,7)
        BlinkField(2, 7, AlarmMinute, 2);
        // 非闪烁部分强制显示
        LCD_ShowNum(2, 4, DisplayHour, 2); // 正常显示小时
        if (AlarmOnOff) {
            LCD_ShowString(2, 11, "ON ");
        } else {
            LCD_ShowString(2, 11, "OFF");
        }
    }
    else if (AlarmFlag == 1) { // 如果光标当前在“开关”位
        // --- 4. 显示开关状态 (位置 2,11) ---
        // 先正常显示时间数字，保证界面完整
        LCD_ShowNum(2, 7, AlarmMinute, 2);
        LCD_ShowNum(2, 4, DisplayHour, 2);

        if (AlarmOnOff) { // 如果当前是开启状态
            LCD_ShowString(2, 11, "   "); // 先显示3个空格（擦除）
            Delay(400); // 延时产生视觉上的闪烁间隔
            LCD_ShowString(2, 11, "ON "); // 再显示 "ON "
            Delay(400); // 延时
        } else { // 如果当前是关闭状态
            LCD_ShowNum(2, 7, AlarmMinute, 2);
            LCD_ShowNum(2, 4, DisplayHour, 2);
            LCD_ShowString(2, 11, "   "); // 先显示3个空格（擦除）
            Delay(400); // 延时
            LCD_ShowString(2, 11, "OFF"); // 再显示 "OFF"
            Delay(400); // 延时
        }
    }
}

void Main_ShowData(void) {
    LCD_ShowString(2, 1, "Main"); // 在LCD第2行第1列显示标题 "Main"

    // 主界面同样需要根据 flag 计算显示小时
    unsigned int DisplayHour; // 定义临时变量，存放最终要在屏幕上显示的小时数
    if (TimeHour_flag == 0) { // 如果当前是24小时制模式
        DisplayHour = RealHour; // 显示的小时数直接等于底层的24小时制时间
    } else { // 如果当前是12小时制模式
        if (RealHour == 0) DisplayHour = 12; // 底层0点转换为显示12 (凌晨12点)
        else if (RealHour > 12) DisplayHour = RealHour - 12; // 13-23点减去12，转换为下午1-11点
        else DisplayHour = RealHour; // 1-12点保持不变直接显示
    }

    // 依次显示时间数据 (从右往左显示，方便对齐)
    LCD_ShowNum(2, 15, TimeSec, 2); // 在第2行第15列显示秒 (2位数)
    LCD_ShowChar(2, 14, ':'); // 在第2行第14列显示秒与分之间的冒号
    LCD_ShowNum(2, 12, TimeMinute, 2); // 在第2行第12列显示分钟 (2位数)
    LCD_ShowChar(2, 11, ':'); // 在第2行第11列显示分与时之间的冒号
    LCD_ShowNum(2, 9, DisplayHour, 2); // 在第2行第9列显示小时 (使用转换后的显示时间)

    // 依次显示日期数据
    LCD_ShowNum(1, 9, TimeDay, 2); // 在第1行第9列显示日 (2位数)
    LCD_ShowChar(1, 8, '-'); // 在第1行第8列显示日与月之间的横杠
    LCD_ShowNum(1, 6, TimeMonth, 2); // 在第1行第6列显示月 (2位数)
    LCD_ShowChar(1, 5, '-'); // 在第1行第5列显示月与年之间的横杠
    LCD_ShowNum(1, 1, TimeYear, 4); // 在第1行第1列显示年 (4位数)
}

// 通用闪烁函数
void BlinkField(unsigned char row, unsigned char col, unsigned int value, unsigned char digits) {
    static unsigned char blink_state = 0; // 定义静态变量保存闪烁状态（0灭1亮），函数退出后值不丢失
    if (TimeCount % 4 == 0) { // 利用全局定时器计数取余，控制闪烁的刷新频率（约每40ms翻转一次）
        blink_state = !blink_state; // 翻转闪烁状态（如果是亮就变灭，如果是灭就变亮）
        if (blink_state) { // 如果当前状态为“亮”
            LCD_ShowNum(row, col, value, digits); // 在指定行列位置显示具体的数值
        } else { // 如果当前状态为“灭”（需要擦除）
            char spaces[5] = "    "; // 准备一个包含4个空格的字符串数组（用于覆盖数字）
            spaces[digits] = '\0'; // 根据传入的位数(digits)，在对应位置截断字符串（比如2位数就只留2个空格）
            LCD_ShowString(row, col, spaces); // 在指定位置显示空格字符串，实现视觉上的“擦除”效果
        }
    }
}

// --- 时间核心逻辑 ---
void Calendar(void) {
    // 秒进位逻辑
    if (TimeSec >= 60) { // 如果当前秒数累计达到或超过60秒
        TimeSec = 0; // 秒数清零，重新开始计数
        TimeMinute++; // 分钟数加1（进位）
    }
    // 分进位逻辑
    if (TimeMinute >= 60) { // 如果当前分钟数累计达到或超过60分钟
        TimeMinute = 0; // 分钟数清零
        RealHour++; // 底层24小时制的小时数加1（进位）
    }
    // 时进位逻辑 (底层逻辑永远是 0-23)
    if (RealHour >= 24) { // 如果小时数累计达到或超过24小时
        RealHour = 0; // 小时数清零（新的一天开始）
        TimeDay++; // 日期（天数）加1（进位）
    }

    // 日期进位逻辑 (处理月份和年份的跨月/跨年)
    unsigned char maxDay = GetDaysInMonth(TimeYear, TimeMonth); // 调用函数，获取当前年份和月份对应的最大天数（如28、30或31天）
    if (TimeDay > maxDay) { // 如果当前日期超过了该月的最大天数
        TimeDay = 1; // 日期重置为1号
        TimeMonth++; // 月份加1（进位到下个月）
        if (TimeMonth > 12) { // 如果月份超过了12月
            TimeMonth = 1; // 月份重置为1月
            TimeYear++; // 年份加1（进入新的一年）
        }
    }
}

// 获取月份天数
unsigned char GetDaysInMonth(unsigned int year, unsigned char month) {
    if (month == 2) { // 如果传入的月份是2月
        // 判断是否为闰年：能被4整除且不能被100整除，或者能被400整除
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) return 29; // 满足闰年条件，2月返回29天
        else return 28; // 平年，2月返回28天
    } else if (month == 4 || month == 6 || month == 9 || month == 11) { // 如果是4、6、9、11这几个小月
        return 30; // 返回30天
    } else { // 剩下的就是1、3、5、7、8、10、12这几个大月
        return 31; // 返回31天
    }
}


// --- 按键逻辑 ---
void KeyLogic(int key) {
    switch (key) {
        // 确认键 (用于切换界面)
        case 1:
            if (Menu_flag == 0) { // 如果当前在主界面
                LCD_Clear();      // 清屏
                SwopAlter();      // 调用函数：将当前真实时间拷贝到临时变量中（准备开始修改）
                Menu_flag = 1;    // 将界面标志位设为1，进入时间设置界面
            } else {              // 如果当前在设置界面（非主界面）
                LCD_Clear();      // 清屏
                AlterSwop();      // 调用函数：将修改后的临时变量写回真实时间（保存修改）
                Menu_flag = 0;    // 将界面标志位设为0，返回主界面
            }
            break;

        // 菜单/返回键
        case 2:
            if (AlarmTriggered == 0) { // 如果闹钟当前没有触发（未响铃）
                if (Menu_flag == 0) {  // 在主界面按下
                    LCD_Clear();
                    Menu_flag = 2;     // 进入闹钟设置界面
                } else {               // 在其他界面按下
                    LCD_Clear();
                    Menu_flag = 0;     // 直接返回主界面
                }
                break;
            } else if (AlarmTriggered == 1) { // 如果闹钟正在响铃
                AlarmTriggered = 0;    // 将闹钟触发标志清零
                buzzer(0);             // 关闭蜂鸣器（消音）
                break;
            }
            break;

        // 切换时制键 (12/24小时制切换)
        case 3:
            TimeHour_flag = !TimeHour_flag; // 直接对标志位取反（0变1，1变0）
            // 注意：这里不需要修改底层时间 RealHour，下次显示时会自动根据新的 flag 进行格式转换
            break;

        // 加键 (数值增加)
        case 4:
            if (Menu_flag == 1) { // 如果在时间设置界面
                switch (AlterFlag) { // 根据光标所在的位置进行增加
                    case 1: if (++AlterSec > 59) AlterSec = 0; break; // 秒加1，超过59归零
                    case 2: if (++AlterMinute > 59) AlterMinute = 0; break; // 分加1，超过59归零
                    // 修改底层小时 AlterRealHour (0-23)
                    case 3: if (++AlterRealHour > 23) AlterRealHour = 0; break;
                    case 4: // 日加1
                        if (++AlterDay > GetDaysInMonth(AlterYear, AlterMonth)) {
                            AlterDay = 1; // 超过当月最大天数则回到1号
                        }
                        break;
                    case 5: // 月加1
                        if (++AlterMonth > 12) AlterMonth = 1; // 超过12月回到1月
                        // 核心修复：防止从31号加到只有30天的月份（如4月）导致日期非法
                        {
                            unsigned char maxDay = GetDaysInMonth(AlterYear, AlterMonth);
                            if (AlterDay > maxDay) {
                                AlterDay = maxDay; // 自动修正为该月的最大天数
                            }
                        }
                        break;
                    case 6: if (++AlterYear > 9999) AlterYear = 1900; break; // 年加1，超过9999回到1900
                }
                break;
            }
            if (Menu_flag == 2) { // 如果在闹钟设置界面
                switch (AlarmFlag) {
                    case 2: if (++AlarmMinute > 59) AlarmMinute = 0; break; // 闹钟分加1
                    case 3: if (++AlarmHour > 23) AlarmHour = 0; break;     // 闹钟时加1
                    case 1: // 切换闹钟开关状态
                        if (AlarmOnOff == 1) { AlarmOnOff = 0; break; }
                        else { AlarmOnOff = 1; break; }
                }
            }
            break;

        // 减键 (数值减少)
        case 7:
            if (Menu_flag == 1) { // 如果在时间设置界面
                switch (AlterFlag) {
                    case 1: if (AlterSec-- == 0) AlterSec = 59; break; // 秒减1，到0回绕到59
                    case 2: if (AlterMinute-- == 0) AlterMinute = 59; break; // 分减1，到0回绕到59
                    case 3: // 底层小时减1，到0回绕到23
                        if (AlterRealHour-- == 0) AlterRealHour = 23;
                        break;
                    case 4: // 日减1，到1号回绕到当月最大天数
                        if (AlterDay-- == 1) AlterDay = GetDaysInMonth(AlterYear, AlterMonth);
                        break;
                    case 5: // 月减1
                        if (AlterMonth == 1) AlterMonth = 12; // 1月减1回到12月
                        else AlterMonth--;
                        // 同样进行日期合法性修正
                        {
                            unsigned char maxDay = GetDaysInMonth(AlterYear, AlterMonth);
                            if (AlterDay > maxDay) {
                                AlterDay = maxDay;
                            }
                        }
                        break;
                    case 6: if (AlterYear-- == 1900) AlterYear = 9999; break; // 年减1，到1900回绕到9999
                }
                break;
            }
            if (Menu_flag == 2) { // 如果在闹钟设置界面
                switch (AlarmFlag) {
                    case 2: // 闹钟分减1
                        if (AlarmMinute == 0) AlarmMinute = 59;
                        else AlarmMinute--;
                        break;
                    case 3: // 闹钟时减1
                        if (AlarmHour == 0) AlarmHour = 23;
                        else AlarmHour--;
                        break;
                    case 1: // 切换闹钟开关状态
                        if (AlarmOnOff == 1) { AlarmOnOff = 0; break; }
                        else { AlarmOnOff = 1; break; }
                }
            }
            break;

        // 光标移动键
        case 5: // 右移键
            if (Menu_flag == 1) { // 在时间设置界面
                AlterFlag++; // 光标位置标志位加1
                if (AlterFlag > 6) AlterFlag = 1; // 超过6（年）则回到1（秒）
                break;
            }
            if (Menu_flag == 2) { // 在闹钟设置界面
                AlarmFlag++; // 光标位置标志位加1
                if (AlarmFlag > 3) AlarmFlag = 1; // 超过3（时）则回到1（开关）
                break;
            }
            break;

        case 6: // 左移键
            if (Menu_flag == 1) { // 在时间设置界面
                AlterFlag--; // 光标位置标志位减1
                if (AlterFlag < 1) AlterFlag = 6; // 小于1（秒）则回到6（年）
                break;
            }
            if (Menu_flag == 2) { // 在闹钟设置界面
                AlarmFlag--; // 光标位置标志位减1
                if (AlarmFlag < 1) AlarmFlag = 3; // 小于1（开关）则回到3（时）
                break;
            }
            break;
    }
    KeyValue = 0; // 处理完当前按键后，将键值清零，防止一次按键被重复识别
}

// --- 硬件初始化与中断 ---
void Timer0Init(void) {
    TMOD &= 0xF0;       // 清除定时器0的工作模式设置（保留定时器1的设置，只修改低4位）
    TMOD |= 0x01;       // 设置定时器0为工作方式1（16位定时器模式）
    TH0 = 64535 / 256;  // 设置定时初值高8位 (64535 = 65536 - 1000，定时1ms)
    TL0 = 64535 % 256 + 1; // 设置定时初值低8位 (加1是为了补偿中断响应的时间误差)
    TF0 = 0;            // 清除定时器0溢出标志位
    TR0 = 1;            // 启动定时器0开始计时
    ET0 = 1;            // 开启定时器0中断允许
    EA = 1;             // 开启全局中断总开关
}
// 定时器0的中断服务函数（__interrupt (1) 表示这是定时器0的中断入口）
void Timer0_Rountine(void) __interrupt (1)
{
    TH0 = 64535 / 256;           // 重新装载定时器高8位初值（用于精确定时）
    TL0 = 64535 % 256 + 1;       // 重新装载定时器低8位初值（+1是为了补偿中断响应和赋值的耗时）
    TimeCount++;                 // 全局计数变量自增，作为系统的软件时基

    if(TimeCount % 5 == 0)       // 每隔5个定时周期（约50ms）执行一次
    {
        // keyScanReady = 1;     // （被注释掉的代码）原本用于标记可以进行按键扫描
    }

    if(TimeCount >= 100)
    {
        TimeSec++;               // 系统的秒数变量加1
        TimeCount = 0;           // 计数变量清零，开始下一秒的重新计数
    }
}

// 蜂鸣器控制函数
void buzzer(int state) {
    if(state) P2_3 = 1;          // 如果传入的 state 为非0（真），则将P2.3引脚置高电平，驱动蜂鸣器响
    else P2_3 = 0;               // 如果传入的 state 为0（假），则将P2.3引脚置低电平，关闭蜂鸣器
}