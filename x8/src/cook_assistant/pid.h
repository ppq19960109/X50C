#ifndef _PID_H_
#define _PID_H_
#ifdef __cplusplus
extern "C"
{
#endif
typedef struct
{
    float Kp; //比例系数Proportional
    float Ki; //积分系数Integral
    float Kd; //微分系数Derivative
} PID_ParaDef;

typedef struct
{
    short Ek;   //当前误差
    short Ek1;  //前一次误差 e(k-1)
    short Ek2;  //再前一次误差 e(k-2)
    int LocSum; //累计积分位置
    short Target_value;
} PID_TypeDef;

short PID_Loc(short SetValue, short ActualValue, PID_ParaDef *PID_Para, PID_TypeDef *PID_Type);
short PID_Inc(short SetValue, short ActualValue, PID_ParaDef *PID_Para, PID_TypeDef *PID_Type);

#ifdef __cplusplus
}
#endif
#endif