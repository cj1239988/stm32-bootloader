#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "bl_usart.h"
#include "ringbuffer.h"
#include "crc16.h"
#include "tim_delay.h"

#define RX_BUFFER_SIZE 1024
#define PACKET_SIZE_MAX 4096
#define RX_TIMEOUT_MS  20
typedef enum {
    PACKET_STATE_HEADER,
    PACKET_STATE_OPCODE,
    PACKET_STATE_LENGTH,
    PACKET_STATE_PAYLOAD,
    PACKET_STATE_CRC16,
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
    //处理字节数据超时接收
    static uint64_t last_byte_ms;
    uint64_t now_ms = tim_get_ms();
    if (now_ms - last_byte_ms > RX_TIMEOUT_MS)
    {
        if(packet_state != PACKET_STATE_HEADER)
        {
            printf("last packet rx timeout\n");
        }
        // 超时处理：重置状态机
        packet_index = 0;
        packet_state = PACKET_STATE_HEADER;
    }
    last_byte_ms = now_ms;

    printf("recv: %02X\n", byte);

    //字节接收状态机处理
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
                printf("payload received ok\n");

                packet_state = PACKET_STATE_CRC16;
            }
            break;
        case PACKET_STATE_CRC16:
            if(packet_index == 4 + packet_payload_length+2)
            {
                uint16_t crc = (packet_buffer[4 + packet_payload_length + 1] << 8) | packet_buffer[4 + packet_payload_length];
                uint16_t ccrc = crc16(packet_buffer, 4 + packet_payload_length);
                if(crc == ccrc)
                {
                    printf("crc16 ok:%04x\n",crc);
                    printf("packet received: opcode=%02X, length=%d\n", packet_opcode, packet_payload_length);
                    printf("payload: ");
                    for(uint32_t i = 0; i < packet_payload_length; i++)
                    {
                        printf("%02X ", packet_buffer[4 + i]);
                    }
                    printf("\n");
                }
                else
                {
                    // 错误处理：CRC校验失败
                    printf("crc16 error: expected %04X, got %04X\n", crc, ccrc);
                }
                packet_index=0;
                packet_state=PACKET_STATE_HEADER;
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


