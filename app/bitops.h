#ifndef __BITOPS_H__
#define __BITOPS_H__
#include <stdint.h>
#include "stm32f4xx.h"


static void put_u8(uint8_t *ptr,  uint8_t data)
{
    ptr[0] = data;
}

static void put_u16(uint8_t *ptr, uint16_t data)
{
    ptr[0] = (uint8_t)(data & 0xFF);
    ptr[1] = (uint8_t)((data >> 8) & 0xFF);
}

static void put_u32(uint8_t *ptr, uint32_t data)
{
    ptr[0] = (uint8_t)(data & 0xFF);
    ptr[1] = (uint8_t)((data >> 8) & 0xFF);
    ptr[2] = (uint8_t)((data >> 16) & 0xFF);
    ptr[3] = (uint8_t)((data >> 24) & 0xFF);
}
static void put_iu8(uint8_t *ptr,  int8_t data)
{
    ptr[0] = (uint8_t)(data&0xFF);
}

static void put_iu16(uint8_t *ptr, int16_t data)
{
    ptr[0] = (uint8_t)(data & 0xFF);
    ptr[1] = (uint8_t)((data >> 8) & 0xFF);
}

static void put_iu32(uint8_t *ptr, int32_t data)
{
    ptr[0] = (uint8_t)(data & 0xFF);
    ptr[1] = (uint8_t)((data >> 8) & 0xFF);
    ptr[2] = (uint8_t)((data >> 16) & 0xFF);
    ptr[3] = (uint8_t)((data >> 24) & 0xFF);
}

#endif /*__BITOPS_H__*/
