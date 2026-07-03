// 发送数据包
// Op->=operation opcode操作码
// 	HEADER	OPCODE	LENGTH	PAYLOAD
// 长度（bytes）	1	1	2	n
// 内容	0xAA	参考OPCODE	表示PAYLOAD的内容长度
// OPCODE
// 	NUM	PARAM	NOTE
// ERASE（擦）	0x01	addr:地址（4bytes）
// size:大小（4bytes）	擦除指定Flash区域内容
// PROGRAM（写）	0x02	addr:地址（4bytes）
// size:大小（4bytes）
// data:待写入的数据	将data写入到addr地址，写入长度为size
// VERIFY（校验）	0x03	addr:地址（4bytes）
// size:大小（4bytes）
// crc:校验和（4bytes）	校验Flash内容
// BOOT
// (boot跳转主程序的步骤)	0x04	addr:地址（4bytes）
// 	从addr处引导主程序
// 示例
// //从0x08000000处擦除1024个字节的Flash的数据包原型
// AA 01 08 00 00 00 00 08 00 04 00 00

// //往0x08000020处写入14字节数据，数据内容为01 02 03 04.....0E
// AA 02 16 00 02 00 00 08 14 00 00 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E

// 响应数据包
// 	HEADER	OPCODE	LENGTH	ERRCODE	PAYLOAD
// 长度（bytes）	1	1	2	1	n
// 内容	0x55	对应发送数据包的OPCODE	表示ERRCODE+PAYLOAD的内容长度	参考ERRCODE
// ERRCODE
// 	NUM	NOTE
// OK	0	操作成功
// ERR_OPCODE	1	OPCODE错误
// ERR_OVERFLOW	2	数据接收长度溢出
// ERR_TIMEOUT	3	操作超时
// ERR_FORMAT	4	格式错误
// ERR_VERIFY	5	校验错误
// ERR_PARAM	6	参数错误
// ERR_UNKNOWN	0xFF	未知异常
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "bl_usart.h"
#include "ringbuffer.h"

#define RX_BUFFER_SIZE 1024
#define PACKET_SIZE_MAX 4096

typedef enum {
    PACKET_STATE_HEADER,
    PACKET_STATE_OPCODE,
    PACKET_STATE_LENGTH,
    PACKET_STATE_PAYLOAD,
} packet_state_machine_t;

typedef enum
{
    PACKET_OPCODE_ERASE = 0x01,
    PACKET_OPCODE_PROGRAM = 0x02,
    PACKET_OPCODE_VERIFY = 0x03,
    PACKET_OPCODE_BOOT = 0x04,
}packet_opcode_t;

typedef enum
{
    PACKET_ERRCODE_OK = 0,
    PACKET_ERRCODE_OPCODE = 1,
    PACKET_ERRCODE_OVERFLOW = 2,
    PACKET_ERRCODE_TIMEOUT = 3,
    PACKET_ERRCODE_FORMAT = 4,
    PACKET_ERRCODE_VERIFY = 5,
    PACKET_ERRCODE_PARAM = 6,
    PACKET_ERRCODE_UNKNOWN = 0xff,
}packet_errcode_t;

static uint8_t rb_buffer[RX_BUFFER_SIZE];
static rb_t rxrb;

static uint8_t packet_buffer[PACKET_SIZE_MAX];
static uint32_t packet_index ;// 当前数据包索引
static packet_state_machine_t packet_state = PACKET_STATE_HEADER;
static packet_opcode_t packet_opcode;
static uint16_t packet_payload_length;

static void bl_byte_handler(uint8_t byte)
{
    printf("recv: %02X\n", byte);
    packet_buffer[packet_index++] = byte;// 将接收到的数据存入缓冲区
    switch(packet_state)
    {
        case PACKET_STATE_HEADER:
            if(packet_buffer[0] == 0xAA)
            {
                printf("header ok\n");
                packet_state = PACKET_STATE_OPCODE;
            }
            else
            {
                packet_index = 0; // 重置索引
                packet_state = PACKET_STATE_HEADER; // 重置状态机
            }
            break;
        case PACKET_STATE_OPCODE:
            if(packet_buffer[1] == PACKET_OPCODE_ERASE || packet_buffer[1] == PACKET_OPCODE_PROGRAM || packet_buffer[1] == PACKET_OPCODE_VERIFY || packet_buffer[1] == PACKET_OPCODE_BOOT)
            {
                printf("opcode ok: %02X\n", packet_buffer[1]);
                packet_opcode = (packet_opcode_t)packet_buffer[1];
                packet_state = PACKET_STATE_LENGTH;
            }
            else
            {
                // 错误处理：无效的操作码
                packet_index = 0; // 重置索引
                packet_state = PACKET_STATE_HEADER; // 重置状态机
            }
            break;
        case PACKET_STATE_LENGTH:
            if(packet_index == 4) // 长度字段为2字节
            {

                uint16_t payload_length = (packet_buffer[3] << 8)|packet_buffer[2] ;
                if(payload_length <= PACKET_SIZE_MAX - 4) // 检查是否溢出
                {
                    printf("length ok: %d\n", payload_length);
                    packet_payload_length = payload_length;
                    packet_state = PACKET_STATE_PAYLOAD; // 进入数据负载状态
                }
                else
                {
                    // 错误处理：数据包长度溢出
                    packet_index = 0; // 重置索引
                    packet_state = PACKET_STATE_HEADER; // 重置状态机
                }
            }
            break;
        case PACKET_STATE_PAYLOAD:
            if(packet_index == 4 + packet_payload_length)
            {
                printf("payload ok\n");
                printf("packet received: opcode=%02X, length=%d\n", packet_opcode, packet_payload_length);
                printf("payload: ");
                for(uint32_t i = 0; i < packet_payload_length; i++)
                {
                    printf("%02X ", packet_buffer[4 + i]);
                }
                printf("\n");
                packet_index = 0; // 重置索引
                packet_state = PACKET_STATE_HEADER; // 重置状态机
            }
            break;
        default:
            break;
    }

}

static void bl_usart_rx_handler(const uint8_t *data, uint32_t length)
{

    rb_puts(rxrb, data, length); // 将接收到的数据放入环形缓冲区
}
void bootloader_main(void)
{
    printf("Bootloader started.\n");

    rxrb = rb_new(rb_buffer, RX_BUFFER_SIZE);
    bl_usart_init();
    bl_usart_register_rx_callback(bl_usart_rx_handler); // 注册接收回调函数
    while(1)
    {
        if(!rb_empty(rxrb))
        {
            uint8_t byte;
            rb_get(rxrb, &byte); // 从环形缓冲区获取一个字节
            bl_byte_handler(byte); // 处理接收到的字节
        }

    }

}


