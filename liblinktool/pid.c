#include "pid.h"

short PID_Loc(short SetValue, short ActualValue, PID_ParaDef *PID_Para, PID_TypeDef *PID_Type)
{
    short PIDLoc; //位置

    PID_Type->Ek = SetValue - ActualValue;
    PID_Type->LocSum += PID_Type->Ek; //累计误差

    PIDLoc = PID_Para->Kp * PID_Type->Ek + (PID_Para->Ki * PID_Type->LocSum) + PID_Para->Kd * (PID_Type->Ek - PID_Type->Ek1);

    PID_Type->Ek1 = PID_Type->Ek;
    return PIDLoc;
}

short PID_Inc(short SetValue, short ActualValue, PID_ParaDef *PID_Para, PID_TypeDef *PID_Type)
{
    short PIDInc; //增量

    PID_Type->Ek = SetValue - ActualValue;
    PIDInc = (PID_Para->Kp * (PID_Type->Ek - PID_Type->Ek1)) - (PID_Para->Ki * PID_Type->Ek) + (PID_Para->Kd * (PID_Type->Ek - 2 * PID_Type->Ek1 + PID_Type->Ek2));

    PID_Type->Ek2 = PID_Type->Ek1;
    PID_Type->Ek1 = PID_Type->Ek;
    return PIDInc;
}