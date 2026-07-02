#include "stm32f4xx.h"
#include "bl_usart.h"
//USE USART3
//RX->PB11
//TX->PB10
//MODE:8N1
//BAUD:115200
//DMA:TX/RX
//IT:RX/DMA_TX_TC/DMA_RX_TC

//TXPD8
//RXPD9
static bl_usart_rx_callback_t rx_callback ; // 接收回调函数指针


static void usart_io_init(void)//配置 GPIO，让 PB10/PB11 具有 USART3 的发送、接收功能。
{
    GPIO_InitTypeDef GPIO_InitStructure;                // GPIO初始化结构体
    GPIO_StructInit(&GPIO_InitStructure);               // 初始化为默认值

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9; // 选择PB10(TX)、PB11(RX)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;             // 设置为复用功能模式
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;        // GPIO速度50MHz
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;           // 推挽输出
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;             // 上拉输入
    GPIO_Init(GPIOD, &GPIO_InitStructure);                   // 初始化GPIOB

    GPIO_PinAFConfig(GPIOD, GPIO_PinSource8, GPIO_AF_USART3); // PB10复用为USART3_TX
    GPIO_PinAFConfig(GPIOD, GPIO_PinSource9, GPIO_AF_USART3); // PB11复用为USART3_RX

}

// static void usart_dma_config(void)//配置 DMA，让串口可以通过 DMA 自动收发数据，减少 CPU 干预。
// {
//     DMA_InitTypeDef DMA_InitStructure;                  // DMA初始化结构体
//     DMA_StructInit(&DMA_InitStructure);                 // 初始化默认值

//     //================ RX DMA =================

//     DMA_InitStructure.DMA_Channel = DMA_Channel_4;                  // USART3对应DMA通道4
//     DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART3->DR); // 外设地址：USART3数据寄存器
//     DMA_InitStructure.DMA_Memory0BaseAddr = 0;                      // 内存地址（后续再设置）
//     DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;         // 数据方向：外设->内存
//     DMA_InitStructure.DMA_BufferSize = 0;                           // 缓冲区大小（后续设置）
//     DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;// 外设地址固定
//     DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;         // 内存地址递增
//     DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设按字节传输
//     DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;         // 内存按字节存储
//     DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                   // 普通模式
//     DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;           // 中等优先级
//     DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;           // 开启FIFO
//     DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;   // FIFO满再传输
//     DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_INC16;      // 内存16次突发
//     DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; // 外设单次传输
//     DMA_Init(DMA1_Stream1, &DMA_InitStructure);                     // 初始化DMA1_Stream1
//     //DMA_ITConfig(DMA1_Stream1, DMA_IT_TC, ENABLE);                       // 使能DMA接收完成中断
//     DMA_Cmd(DMA1_Stream1, DISABLE);                                 // 默认关闭DMA
//                 // 开启USART接收DMA

//     //================ TX DMA =================

//     DMA_InitStructure.DMA_Channel = DMA_Channel_4;                  // USART3对应DMA通道4
//     DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(USART3->DR); // 外设地址：USART3数据寄存器
//     DMA_InitStructure.DMA_Memory0BaseAddr = 0;                      // 内存地址（后续设置）
//     DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;         // 数据方向：内存->外设
//     DMA_InitStructure.DMA_BufferSize = 0;                           // 缓冲区大小（后续设置）
//     DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;// 外设地址固定
//     DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;         // 内存地址递增
//     DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte; // 外设按字节传输
//     DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;         // 内存按字节存储
//     DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;                   // 普通模式
//     DMA_InitStructure.DMA_Priority = DMA_Priority_Medium;           // 中等优先级
//     DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Enable;           // 开启FIFO
//     DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;   // FIFO满再传输
//     DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_INC16;      // 内存16次突发
//     DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; // 外设单次传输
//     DMA_Init(DMA1_Stream3, &DMA_InitStructure);                     // 初始化DMA1_Stream3
//     //DMA_ITConfig(DMA1_Stream3, DMA_IT_TC, ENABLE);                       // 使能DMA发送完成中断
//     DMA_Cmd(DMA1_Stream3, DISABLE);                                 // 默认关闭DMA
//           // 开启USART发送DMA
// }

static void usart_it_config(void)//配置 NVIC 中断，使能 USART3 接收中断和 DMA 发送接收完成中断。
{
    NVIC_InitTypeDef NVIC_InitStructure;              // NVIC中断初始化结构体

    //================ USART3 接收中断 =================

    //RX
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;               // USART3中断号
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;       // 抢占优先级5
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;              // 子优先级0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                 // 使能中断
    NVIC_Init(&NVIC_InitStructure);                                 // 初始化NVIC
    NVIC_SetPriority(USART3_IRQn, 5);                               // 设置中断优先级

    // //================ DMA发送完成中断 =================

    // //DMA TX
    // NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream3_IRQn;         // DMA1_Stream3中断号
    // NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;       // 抢占优先级5
    // NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;              // 子优先级0
    // NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                 // 使能中断
    // NVIC_Init(&NVIC_InitStructure);                                 // 初始化NVIC
    // NVIC_SetPriority(DMA1_Stream3_IRQn,5);

    // //================ DMA接收中断 =================

    // //MDA RX
    // NVIC_InitStructure.NVIC_IRQChannel = DMA1_Stream1_IRQn;         // DMA1_Stream1中断号
    // NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;       // 抢占优先级5
    // NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;              // 子优先级0
    // NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                 // 使能中断
    // NVIC_Init(&NVIC_InitStructure);                                 // 初始化NVIC
    // NVIC_SetPriority(DMA1_Stream1_IRQn,5);

}

static void usart_lowlevel_init(void)//配置 USART3 本体参数。
{
    USART_InitTypeDef USART_InitStructure;            // USART初始化结构体
    USART_StructInit(&USART_InitStructure);           // 初始化为默认值

    USART_InitStructure.USART_BaudRate=115200;
    USART_InitStructure.USART_WordLength=USART_WordLength_8b;
    USART_InitStructure.USART_StopBits=USART_StopBits_1;
    USART_InitStructure.USART_Parity=USART_Parity_No;
    USART_InitStructure.USART_Mode=USART_Mode_Rx | USART_Mode_Tx;
    USART_InitStructure.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
    USART_Init(USART3, &USART_InitStructure);
    // USART_DMACmd(USART3, USART_DMAReq_Rx, ENABLE); // 使能 USART3 接收DMA
    // USART_DMACmd(USART3, USART_DMAReq_Tx, ENABLE); // 使能 USART3 发送DMA
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE); // 使能 USART3 接收中断
    USART_Cmd(USART3, ENABLE); // 使能 USART3
}

void bl_usart_init(void)//串口总初始化函数。
{
    // usart_dma_config();     // 配置 DMA
    usart_it_config();      // 配置 NVIC 中断
    usart_lowlevel_init();  // 配置 USART3
    usart_io_init();        // 配置 GPIO
}

//TLV
//TAG/TYPE Length Value


void bl_usart_write(const uint8_t *data, uint32_t size)//串口发送函数。
{

    while(size--)
    {
        USART_SendData(USART3, *data++); // 发送数据
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET); // 等待当前字节发送完成

    }

    //DMA Transfer 65536 bytes at most
    // while(size > 0)
    // {
    //     uint32_t chunk = (size > 65536) ? 65536 : size; // 每次传输不超过65536字节
    //     DMA1_Stream3->M0AR = (uint32_t)data; // 设置DMA内存地址
    //     DMA1_Stream3->NDTR = chunk;          // 设置DMA传输数据量
    //     DMA_Cmd(DMA1_Stream3, ENABLE);       // 使能DMA传输
    //     while(DMA_GetCmdStatus(DMA1_Stream3) == ENABLE); // 等待
    //     data += chunk;                       // 移动数据指针
    //     size -= chunk;                       // 减少剩余数据量
    // }
}

void bl_usart_register_rx_callback(bl_usart_rx_callback_t callback)//设置接收回调函数。
{
    rx_callback = callback;
}
void USART3_IRQHandler(void)//USART3 接收中断函数。
{
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) // 接收中断
    {
        if(rx_callback) // 如果回调函数已注册
        {
            uint8_t data = USART_ReceiveData(USART3); // 读取接收到的数据
            rx_callback(&data, 1); // 调用回调函数，传递数据和长度
        }
        USART_ClearITPendingBit(USART3, USART_IT_RXNE); // 清除中断标志
    }
}

// void DMA1_Stream3_IRQHandler(void)//DMA 发送完成中断函数。
// {
//     if(DMA_GetITStatus(DMA1_Stream3, DMA_IT_TC) != RESET) // 发送完成中断
//     {
//         DMA_ClearITPendingBit(DMA1_Stream3, DMA_IT_TC); // 清除中断标志

//     }
// }

// void DMA1_Stream1_IRQHandler(void)//DMA 接收完成中断函数。
// {
//     if(DMA_GetITStatus(DMA1_Stream1, DMA_IT_TC) != RESET) // 接收完成中断
//     {
//         DMA_ClearITPendingBit(DMA1_Stream1, DMA_IT_TC); // 清除中断标志

//     }
// }
