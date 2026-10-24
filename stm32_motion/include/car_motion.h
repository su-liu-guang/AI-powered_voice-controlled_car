#ifndef CAR_MOTION_H
#define CAR_MOTION_H
#include <stdint.h>
typedef struct {
    int16_t left;
    int16_t right;
} CarMotion;
/* 正数前进，负数后退，零停止；输出是 PWM 百分比。 */
CarMotion CarMotion_Decode(uint8_t state, uint8_t speed);
#endif
