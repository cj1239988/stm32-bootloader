#ifndef __KEY_DESC_H__
#define __KEY_DESC_H__

#include <stdint.h>
#include "stm32f4xx.h"
#include "key.h"

struct key_desc
{
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIOPuPd_TypeDef pupd;
    BitAction press_level;// 按键按下的电平状态，Bit_SET表示高电平按下，Bit_RESET表示低电平按下

};

#endif /* __KEY_H__ */
