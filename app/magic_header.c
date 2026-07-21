#include <stdbool.h>
#include <stdint.h>
#include "crc32.h"
#include "utils.h"
#include "magic_header.h"

#define MAGIC_HEADER_MAGIC 0x4D414749//MAGI
#define MAGIC_HEADER_ADDR 0x0800C000//魔术头地址

typedef struct {
    uint32_t magic;//魔数,用于标识这是一个有效的魔术头
    uint32_t bitmask;//位掩码，用于标识哪些字段是有效的
    uint32_t reserved1[6];//保留字段，供将来使用

    uint32_t data_type;//类型，根据type类型选择固件下载位置
    uint32_t data_offset;//固件文件相当于magic_header的偏移地址
    uint32_t data_address;//固件实际写入的地址
    uint32_t data_length;//固件长度
    uint32_t data_crc32;//固件的CRC32校验值
    uint32_t reserved2[11];//保留字段，供将来使用

    char version[128];//固件版本字符串

    uint32_t reserved3[6];//保留字段，供将来使用
    uint32_t this_address;//该结构体存储介质的实际地址
    uint32_t this_crc32;//该结构体本身的CRC32校验值

} magic_header_t;

bool magic_header_validate(void)
{
    magic_header_t *header = (magic_header_t *)MAGIC_HEADER_ADDR;//获取魔术头结构体指针
    // 检查魔数是否正确
    if(header->magic != MAGIC_HEADER_MAGIC)
    {
        return false;
    }
    // 检查crc32
    uint32_t ccrc = crc32((uint8_t *)header,offsetof(magic_header_t, this_crc32));//计算结构体除this_crc32字段外的CRC32校验值
    if(ccrc != header->this_crc32)
    {
        return false;
    }

    return true;
}

magic_header_type_t magic_header_get_type(void)
{
    magic_header_t *header = (magic_header_t *)MAGIC_HEADER_ADDR;
    return (magic_header_type_t)header->data_type;
}
uint32_t magic_header_get_offset(void)
{
    magic_header_t *header = (magic_header_t *)MAGIC_HEADER_ADDR;
    return header->data_offset;
}
uint32_t magic_header_get_address(void)
{
    magic_header_t *header = (magic_header_t *)MAGIC_HEADER_ADDR;
    return header->data_address;
}

uint32_t magic_header_get_length(void)
{
    magic_header_t *header = (magic_header_t *)MAGIC_HEADER_ADDR;
    return header->data_length;
}
uint32_t magic_header_get_crc32(void)
{
    magic_header_t *header = (magic_header_t *)MAGIC_HEADER_ADDR;
    return header->data_crc32;
}
