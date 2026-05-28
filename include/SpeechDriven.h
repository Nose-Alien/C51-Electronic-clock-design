/**
* @file   SpeechDriven.h
  * @brief  软件延时
  * @author SleetGit
  * @date   2026/4/29.
  */

#ifndef __SPEECHDRIVEN_H__
#define __SPEECHDRIVEN_H__
void InitUART(void);		//9600bps@11.0592MHz
void Uartsend(unsigned char byte);//发送
void SendBeijingTime(unsigned int hour);
void TelTime(unsigned int Hour_flag,unsigned int hour, unsigned int minute, unsigned int sec);
void SendBeijingTime_12H(unsigned int hour);


#endif
