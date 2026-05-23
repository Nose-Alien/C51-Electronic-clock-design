/* MatrixKey.h
 * 按键读取接口。
 * GetKey() 返回按键编号：0 表示无键，1..n 表示对应按键。
 */

#ifndef __MATRIXKEY_H__
#define __MATRIXKEY_H__

unsigned char GetKey(void);

#endif
