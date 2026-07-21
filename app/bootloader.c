#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f4xx.h"
#include "bl_usart.h"
#include "ringbuffer.h"
#include "crc16.h"
#include "crc32.h"
#include "tim_delay.h"
#include "stm32_flash.h"
#include "board.h"
#include "tim_delay.h"
#include "magic_header.h"

#define BL_VERSION   "0.0.1"
#define BL_ADDRESS   0x08000000
#define BL_SIZE      (48*1024)//48KB
#define APP_VTOR_ADDR    0x08010000
#define RX_BUFFER_SIZE 5*1024
#define PAYLOAD_SIZE_MAX (4096+8)// 4KB为program数据最大长度，8字节为program的地址(4)和长度(4)
#define PACKET_SIZE_MAX  (PAYLOAD_SIZE_MAX + 4 + 2) // 4字节头部+2字节CRC16
#define RX_TIMEOUT_MS  20
#define BOOTLOADER_DELAY 3000
typedef enum {
    PACKET_STATE_HEADER,
    PACKET_STATE_OPCODE,
    PACKET_STATE_LENGTH,
    PACKET_STATE_PAYLOAD,
    PACKET_STATE_CRC16,
} packet_state_machine_t;

typedef enum
{
    PACKET_OPCODE_INQUERY = 0x01,
    PACKET_OPCODE_ERASE = 0x81,
    PACKET_OPCODE_PROGRAM = 0x82,
    PACKET_OPCODE_VERIFY = 0x83,
    PACKET_OPCODE_RESET = 0x21,
    PACKET_OPCODE_BOOT = 0x22,
}packet_opcode_t;

typedef enum
{
    INQUERY_SUBCODE_VERSION=0x00,
    INQUERY_SUBCODE_MTU=0x01,
}inquery_subcode_t;

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

static bool application_validate(void)
{
    if(!magic_header_validate())
    {
        printf("magic header invalid\n");
        return false;
    }
    uint32_t addr=magic_header_get_address();
    uint32_t size=magic_header_get_length();
    uint32_t crc=magic_header_get_crc32();
    uint32_t ccrc = crc32((uint8_t *)addr, size);
    if(ccrc!=crc)
    {
        printf("application crc error: expected %08X, got %08X\n", crc, ccrc);
        return false;
    }
    return true;
}

static void boot_application(void)
{
    //安全检查
    if(!application_validate())
    {
        printf("application invalid, cannot boot.\n");
        return;
    }
    printf("booting application... \n");
    tim_delay_ms(2); // 延时以确保响应发送完成

    led_off(led1); // 关闭LED1指示灯
    TIM_DeInit(TIM6); // 停止定时器6
    USART_DeInit(USART1); // 反初始化 USART1
    USART_DeInit(USART3); // 反初始化 USART3

    NVIC_DisableIRQ(TIM6_DAC_IRQn); // 禁用 TIM6 中断
    NVIC_DisableIRQ(USART1_IRQn); // 禁用 USART1 中断
    NVIC_DisableIRQ(USART3_IRQn); // 禁用 USART3 中断

    SCB->VTOR = APP_VTOR_ADDR; // 设置向量表偏移寄存器为应用程序的基地址
    extern void JumpApp(uint32_t base);// 声明跳转到应用程序的函数
    JumpApp(APP_VTOR_ADDR);// 调用跳转函数，传入应用程序的基地址
}

static void bl_response(packet_opcode_t opcode, packet_errcode_t errcode, const uint8_t *data, uint16_t length)
{
    uint8_t *response = packet_buffer;
    response[0] = 0x55; // Header
    response[1] = opcode; // Opcode
    response[2] = errcode; // Error code
    response[3] = (uint8_t)(length & 0xFF); // Length LSB
    response[4] = (uint8_t)((length >> 8) & 0xFF); // Length MSB
    if(length > 0 ) memcpy(&response[5], data, length); // Payload
    uint16_t crc = crc16(response, 5 + length);
    response[5 + length] = (uint8_t)(crc & 0xFF); // CRC LSB
    response[6 + length] = (uint8_t)((crc >> 8) & 0xFF); // CRC MSB

    bl_usart_write(response, 7 + length); // Send the response
}

//static inline void bl_response_ack(packet_opcode_t opcode, packet_errcode_t errcode)
//{
//    bl_response(opcode, errcode, NULL, 0);
//}

static void bl_opcode_inquery_handler(void)
{
    printf("inquery handler\n");

    if(packet_payload_length != 1)
    {
        printf("inquery packet length error\n");
        return;
    }
    uint8_t subcode=packet_buffer[4];
    switch(subcode)
    {
        case INQUERY_SUBCODE_VERSION:
        {
           bl_response(PACKET_OPCODE_INQUERY, PACKET_ERRCODE_OK, (const uint8_t *)BL_VERSION, strlen(BL_VERSION));
            break;
        }
        case INQUERY_SUBCODE_MTU:
        {
            uint8_t bmtu[2]={(uint8_t)(PAYLOAD_SIZE_MAX & 0xFF), (uint8_t)((PAYLOAD_SIZE_MAX >> 8) & 0xFF)};
            bl_response(PACKET_OPCODE_INQUERY, PACKET_ERRCODE_OK, (const uint8_t *)&bmtu, sizeof(bmtu));
            break;
        }
        default:
        {
            printf("unknown inquery subcode: %02X\n", subcode);
            break;
        }
    }

}

static void bl_opcode_erase_handler(void)
{
   printf("erase handler\n");


   if(packet_payload_length != 8)
   {
       printf("erase packet length error:%d\n", packet_payload_length);// 检查数据包长度是否为8字节
       bl_response(PACKET_OPCODE_ERASE, PACKET_ERRCODE_PARAM, NULL, 0);// 发送错误响应
       return;
   }

   uint32_t address=(packet_buffer[4]) | (packet_buffer[5] << 8) | (packet_buffer[6] << 16) | (packet_buffer[7] << 24);
   uint32_t size=(packet_buffer[8]) | (packet_buffer[9] << 8) | (packet_buffer[10] << 16) | (packet_buffer[11] << 24);

   if(address < STM32_FLASH_BASE || address + size > STM32_FLASH_BASE + STM32_FLASH_SIZE|| size == 0)// 检查地址和大小是否在合法范围内
   {
        printf("address 0x%08X,size=%u is out of range\n", address,size);
        bl_response(PACKET_OPCODE_ERASE,PACKET_ERRCODE_PARAM,NULL,0);
        return;

   }

   if(address >= BL_ADDRESS && address < BL_ADDRESS + BL_SIZE)
   {
        printf("address 0x%08X,size=%u is protected\n", address,size);
        bl_response(PACKET_OPCODE_ERASE,PACKET_ERRCODE_PARAM,NULL,0);
        return;
   }

   printf("erase address: 0x%08X, size: %d\n", address, size);

   stm32_flash_unlock();
   stm32_flash_erase(address, size);
   stm32_flash_lock();

   bl_response(PACKET_OPCODE_ERASE, PACKET_ERRCODE_OK, NULL, 0);

}

static void bl_opcode_program_handler(void)
{
   printf("program handler\n");

   if(packet_payload_length <= 8)
   {
       printf("program packet length error:%d\n", packet_payload_length);// 检查数据包长度是否大于8字节
       bl_response(PACKET_OPCODE_PROGRAM, PACKET_ERRCODE_PARAM, NULL, 0);
       return;
   }

   uint32_t address=(packet_buffer[4]) | (packet_buffer[5] << 8) | (packet_buffer[6] << 16) | (packet_buffer[7] << 24);
   uint32_t size=(packet_buffer[8]) | (packet_buffer[9] << 8) | (packet_buffer[10] << 16) | (packet_buffer[11] << 24);
   uint8_t *data = &packet_buffer[12];

   if(address < STM32_FLASH_BASE || address + size > STM32_FLASH_BASE + STM32_FLASH_SIZE|| size == 0)// 检查地址和大小是否在合法范围内
   {
        printf("address 0x%08X,size=%u is out of range\n", address,size);
        bl_response(PACKET_OPCODE_PROGRAM,PACKET_ERRCODE_PARAM,NULL,0);
        return;

   }

   if(address >= BL_ADDRESS && address < BL_ADDRESS + BL_SIZE)
   {
        printf("program address 0x%08X,size=%u is protected\n", address,size);
        bl_response(PACKET_OPCODE_PROGRAM,PACKET_ERRCODE_PARAM,NULL,0);
        return;
   }

   if(size != packet_payload_length - 8)
   {
       printf("program program size mismatch: %u != %u\n", size, packet_payload_length - 8);
       bl_response(PACKET_OPCODE_PROGRAM, PACKET_ERRCODE_PARAM, NULL, 0);
       return;
   }

   printf("program address: 0x%08X, size: %d\n", address, size);

    stm32_flash_unlock();
    stm32_flash_program(address, data, size);
    stm32_flash_lock();

   bl_response(PACKET_OPCODE_PROGRAM, PACKET_ERRCODE_OK, NULL, 0);
}


static void bl_opcode_verify_handler(void)
{
   printf("verify handler\n");

   if(packet_payload_length != 12)
   {
       printf("verify packet length error:%d\n", packet_payload_length);// 检查数据包长度是否大于8字节
       bl_response(PACKET_OPCODE_VERIFY, PACKET_ERRCODE_PARAM, NULL, 0);
       return;
   }

   uint32_t address=(packet_buffer[4]) | (packet_buffer[5] << 8) | (packet_buffer[6] << 16) | (packet_buffer[7] << 24);
   uint32_t size=(packet_buffer[8]) | (packet_buffer[9] << 8) | (packet_buffer[10] << 16) | (packet_buffer[11] << 24);
   uint32_t crc=(packet_buffer[12]) | (packet_buffer[13] << 8) | (packet_buffer[14] << 16) | (packet_buffer[15] << 24);

   if(address < STM32_FLASH_BASE || address + size > STM32_FLASH_BASE + STM32_FLASH_SIZE|| size == 0)// 检查地址和大小是否在合法范围内
   {
        printf("verify address 0x%08X,size=%u is out of range\n", address,size);
        bl_response(PACKET_OPCODE_VERIFY,PACKET_ERRCODE_PARAM,NULL,0);
        return;

   }

   printf("verify address=0x%08X, size=%u, crc=0x%08X\n", address, size, crc);

   uint32_t ccrc = crc32((uint8_t *)address, size);

   if(ccrc!=crc)
   {
       printf("verify crc error: expected %08X, got %08X\n", crc, ccrc);
       bl_response(PACKET_OPCODE_VERIFY, PACKET_ERRCODE_VERIFY, NULL, 0);
       return;
   }
   bl_response(PACKET_OPCODE_VERIFY, PACKET_ERRCODE_OK, NULL, 0);
}
static void bl_opcode_reset_handler(void)
{
    printf("reset handler\n");
    bl_response(PACKET_OPCODE_RESET, PACKET_ERRCODE_OK, NULL, 0);
    printf("system resetting... \n");
    tim_delay_ms(2); // 延时以确保响应发送完成


    NVIC_SystemReset(); // 调用系统复位函数

}
static void bl_opcode_boot_handler(void)
{
    printf("boot handler\n");
    bl_response(PACKET_OPCODE_BOOT, PACKET_ERRCODE_OK, NULL, 0);
    boot_application();
}
static void bl_packet_handler(void)
{
    switch(packet_opcode)
    {
        case PACKET_OPCODE_INQUERY:
            // 处理查询操作
            bl_opcode_inquery_handler();
            break;
        case PACKET_OPCODE_ERASE:
            // 处理擦除操作
            bl_opcode_erase_handler();
            break;
        case PACKET_OPCODE_PROGRAM:
            // 处理编程操作
            bl_opcode_program_handler();
            break;
        case PACKET_OPCODE_VERIFY:
            // 处理校验操作
            bl_opcode_verify_handler();
            break;
        case PACKET_OPCODE_RESET:
            // 处理复位操作
            bl_opcode_reset_handler();
            break;
        case PACKET_OPCODE_BOOT:
            // 处理启动操作
            bl_opcode_boot_handler();
            break;
        default:
            // 错误处理：未知的操作码
            printf("unknown opcode: %02X\n", packet_opcode);
            break;
    }
}

static bool bl_byte_handler(uint8_t byte)
{
    bool full_packet = false;//表示有没有收到一个完整的数据包
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

    //printf("recv: %02X\n", byte);

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
                printf("header error: %02X\n", packet_buffer[0]);
                packet_index = 0; // 重置索引
                packet_state = PACKET_STATE_HEADER; // 重置状态机
            }
            break;
        case PACKET_STATE_OPCODE:
            if(packet_buffer[1] == PACKET_OPCODE_ERASE || packet_buffer[1] == PACKET_OPCODE_PROGRAM || packet_buffer[1] == PACKET_OPCODE_VERIFY || packet_buffer[1] == PACKET_OPCODE_BOOT|| packet_buffer[1] == PACKET_OPCODE_RESET || packet_buffer[1] == PACKET_OPCODE_INQUERY)
            {
                printf("opcode ok: %02X\n", packet_buffer[1]);
                packet_opcode = (packet_opcode_t)packet_buffer[1];
                packet_state = PACKET_STATE_LENGTH;
            }
            else
            {
                printf("opcode error: %02X\n", packet_buffer[1]);
                // 错误处理：无效的操作码
                packet_index = 0; // 重置索引
                packet_state = PACKET_STATE_HEADER; // 重置状态机
            }
            break;
        case PACKET_STATE_LENGTH:
            if(packet_index == 4) // 长度字段为2字节
            {

                uint16_t payload_length = (packet_buffer[3] << 8)|packet_buffer[2] ;
                if(payload_length <= PACKET_SIZE_MAX) // 检查是否溢出
                {
                    printf("length ok: %d\n", payload_length);
                    packet_payload_length = payload_length;
                    if(payload_length >0)
                        packet_state = PACKET_STATE_PAYLOAD; // 否则进入数据负载状态
                    else
                        packet_state = PACKET_STATE_CRC16; // 如果没有负载，直接进入CRC16状态
                }
                else
                {
                    printf("length error: %d\n", payload_length);
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
                    full_packet = true; // 收到一个完整且有效的数据包
                    printf("crc16 ok:%04x\n",crc);
                    printf("packet received: opcode=%02X, length=%d\n", packet_opcode, packet_payload_length);
                    // printf("payload: ");
                    // for(uint32_t i = 0; i < packet_payload_length; i++)
                    // {
                    //     printf("%02X ", packet_buffer[4 + i]);
                    // }
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
    return full_packet;
}

static void bl_usart_rx_handler(const uint8_t *data, uint32_t length)
{

    rb_puts(rxrb, data, length); // 将接收到的数据放入环形缓冲区
}

static bool key_trap_check(void)
{
    for(uint32_t i=0;i<BOOTLOADER_DELAY;i+=10)
    {
        tim_delay_ms(10); // 延时10毫秒，避免按键抖动
        if(!key_read(key2)) // 检查按键是否按下
            return false; // 如果按键未按下，返回false

    }
    printf("key2 pressed, trap boot mode.\n");
    return true; // 如果按键在延时内一直按下，返回true
}
//等待按键释放
static void wait_key_release(void)
{
    while(key_read(key2))
    {
        tim_delay_ms(10);
    }
}

static bool key_press_check(void)
{
    if(!key_read(key2))
        return false;
    tim_delay_ms(10);//消抖
    if(!key_read(key2))
        return false;
    return true;
}

bool magic_header_trap_boot(void)
{
    if(!application_validate())
    {
        printf("application invalid, trap boot mode.\n");
        return true;
    }
    return false;
}
void bootloader_main(void)
{
    printf("Bootloader started.\n");

    rxrb = rb_new(rb_buffer, RX_BUFFER_SIZE);
    bl_usart_init();
    bl_usart_register_rx_callback(bl_usart_rx_handler);

    key_init(key2);
    bool trapboot=key_trap_check();
    if(!trapboot)
        trapboot=magic_header_trap_boot();
    if(!trapboot)
    {
        boot_application();
    }
    led_init(led1);
    led_on(led1);
    wait_key_release(); // 等待按键释放，避免误触发

    while(1)
    {
        if(key_press_check())
        {
            printf("Key2 pressed, resetting system.\n");
            tim_delay_ms(2); // 延时以确保响应发送完成
            NVIC_SystemReset(); // 调用系统复位函数
        }

        if(!rb_empty(rxrb))
        {
            uint8_t byte;
            rb_get(rxrb, &byte); // 从环形缓冲区获取一个字节
            if(bl_byte_handler(byte)) // 处理接收到的字节
            {
                bl_packet_handler();
            }
        }

    }

}


