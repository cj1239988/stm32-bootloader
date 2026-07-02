#ifndef __BL_USART_H
#define __BL_USART_H
#include <stdint.h>
typedef void (*bl_usart_rx_callback_t)(const uint8_t *data,uint32_t size); // 定义回调函数类型
void bl_usart_init(void);//串口总初始化函数。
void bl_usart_write(const uint8_t *data, uint32_t size);//串口发送函数。
void bl_usart_register_rx_callback(bl_usart_rx_callback_t callback);//设置接收回调函数。
#endif /*__BL_USART_H*/
