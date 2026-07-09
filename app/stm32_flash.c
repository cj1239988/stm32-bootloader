//芯片手册flash介绍：
//HCLK必须大于1MHZ，
//当写flash时发生复位，
//数据内容没法保证，
//当俩不同快在进行读写操作时，
//可以同时执行，
//boot loader执行的时候可以进行另一个块的擦和写
//二机保护:需要连着往这俩寄存器写数据

#include <stdint.h>
#include "stm32f4xx.h"
#include <stdio.h>
#define FLASH_BASE_ADDRESS 0x08000000


typedef struct
{
    uint32_t sector;
    uint32_t size;
}sector_desc_t;

static const sector_desc_t sector_descs[] =
{
    {FLASH_Sector_0, 16*1024},//sector0: 16KB
    {FLASH_Sector_1, 16*1024},//sector1: 16KB
    {FLASH_Sector_2, 16*1024},//sector2: 16KB
    {FLASH_Sector_3, 16*1024},//sector3: 16KB
    {FLASH_Sector_4, 64*1024},//sector4: 64KB
    {FLASH_Sector_5, 128*1024},//sector5: 128KB
    {FLASH_Sector_6, 128*1024},//sector6: 128KB
    {FLASH_Sector_7, 128*1024},//sector7: 128KB
    {FLASH_Sector_8, 128*1024},//sector8: 128KB
    {FLASH_Sector_9, 128*1024},//sector9: 128KB
    {FLASH_Sector_10, 128*1024},// sector10: 128KB
    {FLASH_Sector_11, 128*1024}// sector11: 128KB
};
void stm32_flash_lock(void)
{
    FLASH_Lock();

}
void stm32_flash_unlock(void)
{
    FLASH_Unlock();
}



void stm32_flash_erase(uint32_t address, uint32_t size)
{
    uint32_t addr =FLASH_BASE_ADDRESS;
    for(uint32_t i=0;i<sizeof(sector_descs)/sizeof(sector_desc_t);i++)
    {
        if(addr >= address && addr < address + size)
        {
            printf("erasing sector %u at address 0x%08X size %u\n",i,addr,sector_descs[i].size);
            if(FLASH_EraseSector(sector_descs[i].sector,VoltageRange_3)!=FLASH_COMPLETE)
            {
                printf("flash erase error\n");
            }
        }
        addr+= sector_descs[i].size;
    }

}
void stm32_flash_program(uint32_t address, const uint8_t *data, uint32_t size)
{
    for(uint32_t i=0;i<size;i+=4)
    {
        if(FLASH_ProgramWord(address+i,*(uint32_t *)(data+i)) != FLASH_COMPLETE)
        {
            printf("flash program error at address 0x%08lx\n", address+i);
        }
    }
}
